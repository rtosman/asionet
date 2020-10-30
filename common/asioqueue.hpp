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
        
        typename std::deque<T>::const_iterator begin()
        {
            std::scoped_lock lock(m_queue_mutex);
            return m_queue.begin();
        }

        typename std::deque<T>::const_iterator end()
        {
            std::scoped_lock lock(m_queue_mutex);
            return m_queue.end();
        }

        [[nodiscard]] T& front()
        {
            std::scoped_lock lock(m_queue_mutex);
            return m_queue.front();
        }

        [[nodiscard]] T& back()
        {
            std::scoped_lock lock(m_queue_mutex);
            return m_queue.back();
        }

        void push_back(T& item)
        {
            std::scoped_lock lock(m_queue_mutex);
            m_queue.emplace_back(item);

        }

        void push_front(T& item)
        {
            std::scoped_lock lock(m_queue_mutex);
            m_queue.emplace_front(item);
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

        void pop_front()
        {
            std::scoped_lock lock(m_queue_mutex);
            m_queue.pop_front();
        }

        void pop_back()
        {
            std::scoped_lock lock(m_queue_mutex);
            m_queue.pop_back();
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

        template <typename ...Args>
        [[nodiscard]] T& create_inplace(Args&& ...args)
        {
            std::scoped_lock lock(m_queue_mutex);
            return m_queue.emplace_back(std::forward<Args>(args)...);
        }

    protected:
            std::mutex                      m_queue_mutex;
            std::deque<T>                   m_queue;
    };
}
#endif
