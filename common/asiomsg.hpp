#ifndef _ASIOMSG_HPP_INCLUDED
#define _ASIOMSG_HPP_INCLUDED
#include "asionet.hpp"
#include "asiobuiltin.hpp"
#include "asionetcrypto.hpp"

namespace asionet 
{
    template <typename T>
    struct message_header 
    {
        T           m_id{};
        uint32_t    m_size{ 0 };
        uint8_t     m_iv[AESBlockSize]{};
    };

    template <typename T, typename BodyType = Botan::secure_vector<uint8_t>>
    struct message
    {
        message_header<T>               m_header{};
        BodyType                        m_body;

        message()
        {
        }

        message(const message_header<T>& hdr)
        {
            m_header = hdr;
            if (hdr.m_size == 0)
            {
                blank();
            }
        }

        message(const message<T>& other)
        {
            if(this == &other) return;

            this->m_body = other.m_body;
            this->m_header = other.m_header;
        }

        message(message<T>&& other)
        {
            if(this == &other) return;

            this->m_body = other.m_body;
            this->m_header = other.m_header;

            other.blank();
        }

        message<T>& operator=(const message<T>& other)
        {
            if(this == &other) return *this;

            this->m_body = other.m_body;
            this->m_header = other.m_header;

            return *this;
        }

        BodyType& body()
        {
            return m_body;
        }

        [[nodiscard]] T api() const
        {
            return m_header.m_id;
        }

        void blank()
        {
            m_body.resize(0);
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

        friend message<T>& operator<<(message<T>& msg, const char* data)
        {
            size_t i = msg.m_body.size();
 
            msg.m_body.resize(msg.m_body.size() + strlen(data)+1);

            std::memcpy(msg.m_body.data() + i, data, strlen(data));

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

    template <typename T, bool Encrypt=true>
    struct session;

    template <typename T, bool Encrypt=true>
    struct owned_message
    {
        std::shared_ptr<session<T, Encrypt>>        m_remote;
        message<T>                                  m_msg;
        
        owned_message(const message_header<T>& hdr, std::shared_ptr<session<T, Encrypt>> remote):
                            m_remote(remote),
                            m_msg(hdr)
        {
        }

        owned_message(message<T>& msg, std::shared_ptr<session<T, Encrypt>> remote):
                            m_remote(remote),
                            m_msg(msg)
        {
        }

        owned_message(const owned_message<T, Encrypt>& other)
        {
            if(this == &other) return;

            this->m_remote = other.m_remote;
            this->m_msg = other.m_msg;
            std::cout << "owned_msg copy constructed\n";
        }

        owned_message(owned_message<T, Encrypt>&& other)
        {
            if(this == &other) return;

            this->m_remote = other.m_remote;
            this->m_msg = other.m_msg;

            other.m_remote = nullptr;
            other.m_msg.blank();
            std::cout << "owned_msg move constructed\n";

        }

        owned_message<T, Encrypt>& operator=(const owned_message<T, Encrypt>& other)
        {
            if(this == &other) return *this;

            this->m_remote = other.m_remote;
            this->m_msg = other.m_msg;
        }
    };
}

// following is the support for the authentication challenge
struct onetx
{
    uint8_t x;
    uint8_t y;
};

std::ostream& operator<<(std::ostream& o, onetx const& p)
{  
    return o << (int)p.x << ", " << (int)p.y;  
}

constexpr onetx genslider(std::size_t curr, std::size_t total)
{
    return {(uint8_t)(curr*13/(total-1)), (uint8_t)(curr*19/total)};
}

template<typename T, int N>
uint8_t slide(uint8_t in, std::array<T,N> arr)
{
    for (auto p: arr)
    {
        if (p.x == (in&0xf))
        {
            return p.y;
        }
    }

    return 0;
}

#endif
