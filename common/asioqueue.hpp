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

        const std::shared_ptr<T> front()
        {
            std::scoped_lock lock(m_queue_mutex);
            return m_queue.front();
        }

        const std::shared_ptr<T> back()
        {
            std::scoped_lock lock(m_queue_mutex);
            return m_queue.back();
        }

        void push_back(std::shared_ptr<T> item)
        {
            std::scoped_lock lock(m_queue_mutex);
            m_queue.emplace_back(item);

        }

        void push_front(std::shared_ptr<T> item)
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

        [[nodiscard]] std::shared_ptr<T> pop_front()
        {
            std::scoped_lock lock(m_queue_mutex);
            auto t = m_queue.front();
            m_queue.pop_front();
            return t;
        }

        [[nodiscard]] std::shared_ptr<T> pop_back()
        {
            std::scoped_lock lock(m_queue_mutex);
            auto t = m_queue.back();
            m_queue.pop_back();
            return t;
        }

        typename std::deque<T>::iterator erase(typename std::deque<T>::const_iterator i)
        {
            std::scoped_lock lock(m_queue_mutex);
            return m_queue.erase(i);
        }

        void slow_erase(const std::shared_ptr<T>& item)
        {
            std::scoped_lock lock(m_queue_mutex);
            for(auto i = m_queue.begin(); i != m_queue.end(); )
            {
                ((*i) == item)? i = m_queue.erase(i):++i;
            }
        }

        [[nodiscard]] std::shared_ptr<T>& create_empty_inplace()
        {
            std::scoped_lock lock(m_queue_mutex);
            return m_queue.emplace_back(std::make_shared<T>());
        }

        [[nodiscard]] std::shared_ptr<T>& create_inplace(const T& item)
        {
            std::scoped_lock lock(m_queue_mutex);
            return m_queue.emplace_back(std::make_shared<T>(item));
        }

    protected:
            std::mutex                      m_queue_mutex;
            std::deque<std::shared_ptr<T>>  m_queue;
    };
}
#endif
