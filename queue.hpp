#ifndef _queue_hpp_
#define _queue_hpp_

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>

template <typename T, typename Container = std::deque<T>>
class queue
{
public:
    queue() : m_disabled(false) {}

    ~queue()
    {
        disable();
    }

    bool put(const T & obj)
    {
        if (disabled())
        {
            return false;
        }

        std::unique_lock<std::mutex> lock(m_mutex);

        m_queue.emplace_back(obj);
        m_cond.notify_one();

        return true;
    }

    bool get(T & obj)
    {
        if (disabled())
        {
            return false;
        }

        std::unique_lock<std::mutex> lock(m_mutex);

        while (m_queue.empty())
        {
            if (disabled())
            {
                return false;
            }

            m_cond.wait(lock);

            if (disabled())
            {
                return false;
            }
        }

        obj = m_queue.front();
        m_queue.pop_front();

        return true;
    }

    void disable()
    {
        m_disabled.store(true, std::memory_order_relaxed);
        m_cond.notify_all();
    }

    bool disabled() const
    {
        return m_disabled.load(std::memory_order_relaxed);
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_cond;

    Container m_queue;
    std::atomic_bool m_disabled;
};

#endif // _queue_hpp_
