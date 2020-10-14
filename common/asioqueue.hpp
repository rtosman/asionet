#ifndef _ASIOPROTQUEUE_HPP_INCLUDED
#define _ASIOPROTQUEUE_HPP_INCLUDED
#include "asionet.hpp"

namespace asionet 
{
    template<typename T>
    struct protqueue
    {
        protqueue() = default;
        protqueue(const protqueue<T>&) = delete;
        ~protqueue() { clear(); }
        
        const T& front()
        {
            std::scoped_lock lock(m_queue_mutex);
            return m_queue.front();
        }

        const T& back()
        {
            std::scoped_lock lock(m_queue_mutex);
            return m_queue.back();
        }

        void push_back(const T& item)
        {
            std::scoped_lock lock(m_queue_mutex);
            m_queue.emplace_back(std::move(item));

        }

        void push_front(const T& item)
        {
            std::scoped_lock lock(m_queue_mutex);
            m_queue.emplace_front(std::move(item));
        }

        [[nodiscard]] bool empty()
        {
            std::scoped_lock lock(m_queue_mutex);
            return m_queue.empty();
        }

        [[nodiscard]] size_t size()
        {
            std::scoped_lock lock(m_queue_mutex);
            return m_queue.size();
        }

        void clear()
        {
            std::scoped_lock lock(m_queue_mutex);
            m_queue.clear();
        }

        [[nodiscard]] T pop_front()
        {
            std::scoped_lock lock(m_queue_mutex);
            auto t = std::move(m_queue.front());
            m_queue.pop_front();
            return t;
        }

        [[nodiscard]] T pop_back()
        {
            std::scoped_lock lock(m_queue_mutex);
            auto t = std::move(m_queue.back());
            m_queue.pop_back();
            return t;
        }

        typename std::deque<T>::iterator erase(typename std::deque<T>::const_iterator i)
        {
            std::scoped_lock lock(m_queue_mutex);
            return m_queue.erase(i);
        }

        void slow_erase(const T& item)
        {
            std::scoped_lock lock(m_queue_mutex);
            for(auto i = m_queue.begin(); i != m_queue.end(); )
            {
                (&(*i) == &item)? i = m_queue.erase(i):++i;
            }
        }

        [[nodiscard]] T& create_empty_inplace()
        {
            std::scoped_lock lock(m_queue_mutex);
            return m_queue.emplace_back();
        }

        [[nodiscard]] T& create_inplace(const T& item)
        {
            std::scoped_lock lock(m_queue_mutex);
            return m_queue.emplace_back(item);
        }

    protected:
            std::mutex      m_queue_mutex;
            std::deque<T>   m_queue;
    };
}
#endif
