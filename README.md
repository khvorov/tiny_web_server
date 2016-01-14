# tiny_web_server

A simple yet functional web-server for serving static content only. Server is based on epoll event distribution interface.

## Tech

* server uses epoll functionality to process I/O events
* server decouples reception and processing of incoming requests by using an internal thread pool

## Installation

### Compile

```sh
hg clone https://bitbucket.org/dkhvorov/tiny_web_server
cd tiny_web_server
make
```

By default it compiles with GCC, however you can build it with clang by running that command:

```sh
make CC=clang++
```
Project was tested with the following compilers:

* GCC 4.9.1
* clang 3.5

### Run

Use following command to run the application:

```sh
./tiny_web_server <port> <path>
```

You have to specify two parameters - the port to bind and the path to the folder with static content, e.g

```sh
./tiny_web_server 8080 /home/larrypage/work/google.com
```

License
----

Beerware :)

