#ifndef _ASIOMSG_HPP_INCLUDED
#define _ASIOMSG_HPP_INCLUDED
#include "asionet.hpp"

namespace asionet 
{
    template <typename T> 
    struct message_header 
    {
        T           m_id{};
        uint32_t    m_size = 0;
    };

    template <typename T>
    struct message
    {
        message_header<T>       m_header{};
        std::vector<uint8_t>    m_body;

        message()
        {
            m_body.resize(0);
        }

        message(message_header<T>& hdr)
        {
            m_header = hdr;
            m_body.resize(0);
        }

        std::vector<uint8_t>& body()
        {
            return m_body;
        }

        [[nodiscard]] T& api()
        {
            return m_header.m_id;
        }

        friend std::ostream& operator<<(std::ostream& os, const message<T>& msg)
        {
            os << "ID: " << int(msg.m_header.m_id) << " Size: " << msg.m_header.m_size;
            return os;
        }

        template<typename DT>
        friend message<T>& operator<<(message<T>& msg, const DT& data)
        {
            static_assert(std::is_standard_layout<DT>::value, "Data cannot be trivially serialized");

            size_t i = msg.m_body.size();

            msg.m_body.resize(msg.m_body.size() + sizeof(DT));

            std::memcpy(msg.m_body.data() + i, &data, sizeof(DT));

            msg.m_header.m_size = msg.m_body.size();

            return msg;
        }

        template<typename DT>
        friend message<T>& operator>>(message<T>& msg, const DT& data)
        {
            static_assert(std::is_standard_layout<DT>::value, "Data cannot be trivially deserialized");

            size_t i = msg.m_body.size() - sizeof(DT);

            std::memcpy((void*)&data, msg.m_body.data() + i, sizeof(DT));

            msg.m_body.resize(i);

            msg.m_header.m_size = msg.m_body.size();

            return msg;
        }
    };

    template <typename T>
    struct session;

    template <typename T>
    struct owned_message
    {
        std::shared_ptr<session<T>> m_remote;
        message<T>                  m_msg;

        owned_message(message_header<T>& hdr, std::shared_ptr<session<T>> remote):
                            m_remote(remote),
                            m_msg(hdr)
        {

        }

        owned_message(message<T>& msg, std::shared_ptr<session<T>> remote):
                            m_remote(remote),
                            m_msg(msg)
        {

        }

        friend std::ostream& operator<<(std::ostream& os, const owned_message<T>& msg)
        {
            os << msg.msg;
            return os;
        }
    };
}

#endif
