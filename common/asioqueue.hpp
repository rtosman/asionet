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

        [[nodiscard]] size_t count()
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
            return m_queue.erase(i);
        }

        [[nodiscard]] std::tuple<T&, std::deque<T>::const_iterator>
                  create_empty_inplace()
        {
            std::scoped_lock lock(m_queue_mutex);
            T& reply = m_queue.emplace_back();
            auto iter = m_queue.end() - 1;
            return std::make_tuple(std::ref(reply), iter);
        }

    protected:
            std::mutex      m_queue_mutex;
            std::deque<T>   m_queue;
    };
}
#endif
