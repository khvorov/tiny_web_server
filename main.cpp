#include <vector>
#include <fstream>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>

#include "configuration.hpp"
#include "thread_pool.hpp"
#include "request_processor.hpp"

#define MAXEVENTS 128

namespace
{
int make_socket_non_blocking (int sfd)
{
    int flags = fcntl(sfd, F_GETFL, 0);
    if (flags == -1)
    {
        perror("fcntl");
        return -1;
    }

    flags |= O_NONBLOCK;
    int s = fcntl(sfd, F_SETFL, flags);
    if (s == -1)
    {
        perror("fcntl");
        return -1;
    }

    return 0;
}

int create_and_bind (char *port)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int s, sfd;

    memset (&hints, 0, sizeof (struct addrinfo));
    hints.ai_family = AF_UNSPEC;     // Return IPv4 and IPv6 choices
    hints.ai_socktype = SOCK_STREAM; // We want a TCP socket
    hints.ai_flags = AI_PASSIVE;     // All interfaces

    s = getaddrinfo (NULL, port, &hints, &result);
    if (s != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
            continue;

        s = bind(sfd, rp->ai_addr, rp->ai_addrlen);
        if (s == 0)
        {
            // We managed to bind successfully!
            break;
        }

        close(sfd);
    }

    if (rp == NULL)
    {
        fprintf(stderr, "Could not bind\n");
        return -1;
    }

    freeaddrinfo (result);

    return sfd;
}

void process_listening_socket(int sfd, int efd, epoll_event & event)
{
    // We have a notification on the listening socket, which means one or more incoming connections.
    // TODO: check event
    while (true)
    {
        struct sockaddr in_addr;
        socklen_t in_len;
        int infd;
        char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

        in_len = sizeof(in_addr);
        infd = accept(sfd, &in_addr, &in_len);

        if (infd == -1)
        {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
            {
                // We have processed all incoming connections.
                break;
            }
            else
            {
                perror("accept");
                break;
            }
        }

        int s = getnameinfo (&in_addr, in_len, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);
        if (s == 0)
        {
            printf("Accepted connection on descriptor %d (host=%s, port=%s)\n", infd, hbuf, sbuf);
        }

        // Make the incoming socket non-blocking and add it to the list of fds to monitor.
        s = make_socket_non_blocking (infd);
        if (s == -1)
            abort();

        event.data.fd = infd;
        event.events = EPOLLIN | EPOLLET;
        s = epoll_ctl(efd, EPOLL_CTL_ADD, infd, &event);
        if (s == -1)
        {
            perror("epoll_ctl");
            abort();
        }
    }
}
}

int main (int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s [port] [path]\n", argv[0]);
        exit (EXIT_FAILURE);
    }

    // parse command line
    configuration::instance().rootPath = argv[2];

    printf("root path: %s\n", configuration::instance().rootPath.c_str());

    int sfd = create_and_bind (argv[1]);
    if (sfd == -1)
        abort();

    int s = make_socket_non_blocking (sfd);
    if (s == -1)
        abort();

    s = listen (sfd, SOMAXCONN);
    if (s == -1)
    {
        perror("listen");
        abort();
    }

    int efd = epoll_create1 (0);
    if (efd == -1)
    {
        perror("epoll_create");
        abort();
    }

    epoll_event event;
    event.data.fd = sfd;
    event.events = EPOLLIN | EPOLLET;

    s = epoll_ctl (efd, EPOLL_CTL_ADD, sfd, &event);
    if (s == -1)
    {
        perror("epoll_ctl");
        abort();
    }

    // Buffer where events are returned
    std::vector<epoll_event> events(MAXEVENTS);

    // thread pool to process incoming requests
    thread_pool<> pool(4);

    printf("starting event loop...\r\n");

    // The event loop
    while (true)
    {
        int n = epoll_wait(efd, &events[0], MAXEVENTS, -1);

        for (int i = 0; i < n; i++)
        {
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN)))
            {
                // An error has occured on this fd, or the socket is not ready for reading (why were we notified then?)
                fprintf(stderr, "epoll error\n");
                close(events[i].data.fd);
                continue;
            }
            else if (sfd == events[i].data.fd)
            {
                // We have a notification on the listening socket, which means one or more incoming connections.
                // TODO: Is execution by the thread pool thread-safe?
                //process_listening_socket(sfd, efd, event);
                pool.execute(std::bind(process_listening_socket, sfd, efd, event));
            }
            else
            {
                // We have data on the fd waiting to be read
                int done = 0;

                ByteBuffer buffer;

                while (1)
                {
                    ssize_t count = 0;
                    char buf[512];

                    count = read(events[i].data.fd, buf, sizeof(buf));
                    if (count == -1)
                    {
                        // If errno == EAGAIN, that means we have read all data. So go back to the main loop.
                        if (errno != EAGAIN)
                        {
                            perror("read");
                            done = 1;
                        }
                        break;
                    }
                    else if (count == 0)
                    {
                        // End of file. The remote has closed the connection.
                        done = 1;
                        break;
                    }

                    std::copy(buf, buf + count, std::back_inserter(buffer));
                }

                if (!buffer.empty())
                {
                    printf("got %lu bytes\n", buffer.size());

                    int fd = events[i].data.fd;

                    request_processor processor;
                    pool.execute(std::bind(processor, std::make_shared<ByteBuffer>(std::move(buffer)), fd));
                }

                if (done)
                {
                    printf("Closed connection on descriptor %d\n", events[i].data.fd);

                    // Closing the descriptor will make epoll remove it from the set of descriptors which are monitored
                    close(events[i].data.fd);
                }
            }
        }
    }

    close(sfd);

    return 0;
}
