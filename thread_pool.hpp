#ifndef _thread_pool_hpp_
#define _thread_pool_hpp_

#include <functional>
#include <list>
#include <thread>

#include "queue.hpp"

template <typename WorkItem = std::function<void ()>>
class thread_pool
{
public:
    thread_pool(std::size_t capacity)
    {
        for (std::size_t i = 0; i < capacity; ++i)
        {
            m_threads.emplace_back([this]() { this->process(); });
        }
    }

    ~thread_pool()
    {
        disable();

        for (auto & t : m_threads)
        {
            t.join();
        }
    }

    void disable()
    {
        m_queue.disable();
    }

    bool execute(WorkItem item)
    {
        return m_queue.put(item);
    }

private:
    void process()
    {
        while (true)
        {
            WorkItem item;

            if (!m_queue.get(item))
            {
                return;
            }

            try
            {
                item();
            }
            catch (std::exception & e)
            {
                fprintf(stderr, "caught an exception while executing on a thread pool\n%s\n", e.what());
            }
            catch (...)
            {
                fprintf(stderr, "caught an unknown exception while executing on a thread pool\n");
            }
        }
    }

private:
    std::list<std::thread> m_threads;
    queue<WorkItem> m_queue;
};

#endif // _thread_pool_hpp_
