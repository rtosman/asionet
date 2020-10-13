#ifndef _ASIOCONNECTION_HPP_INCLUDED
#define _ASIOCONNECTION_HPP_INCLUDED
#include "asionet.hpp"
#include "asioqueue.hpp"
#include "asiomsg.hpp"

namespace asionet 
{
    template <typename T>
    struct connection: public std::enable_shared_from_this<connection<T>>
    {
        connection(asio::io_context& ctxt, 
                   protqueue<message<T>>& inbound): 
                    m_context(ctxt),
                    m_inbound(inbound),
                    m_socket(m_context)
        {
        }
        virtual ~connection()
        {}
        
        [[nodiscard]] bool connect(const asio::ip::tcp::endpoint& endpoint) 
        {
            asio::error_code    ec;

            m_socket.connect(endpoint, ec);
            if(ec) 
            {
                return false;
            }

            return true;
        };

        bool disconnect()
        {
            m_socket.close();
            return true; // TODO
        };

        [[nodiscard]] bool is_connected() const
        {
            return m_socket.is_open();
        };

        [[nodiscard]] bool send(const message<T>& msg)
        {
            asio::error_code    ec;

            m_socket.write_some(asio::buffer(&msg.m_header, sizeof(msg.m_header)), ec);
            if(ec)
            {
                return false;
            }
            m_socket.write_some(asio::buffer(msg.m_body.data(), msg.m_body.size()), ec);
            if(ec)
            {
                return false;
            }

            return true;
        }

        [[nodiscard]] std::shared_ptr<message<T>> response()
        {
            asio::error_code            ec;
            std::shared_ptr<message<T>> resp(std::make_shared<message<T>>());

            size_t n = m_socket.read_some(asio::buffer(&resp->m_header, sizeof(resp->m_header)), ec);
            if(!ec && n == sizeof(resp->m_header))
            {
                if(resp->m_header.m_size > 0) {
                    std::cout << "resizing body to " << resp->m_header.m_size << "\n";
                    resp->m_body.resize(resp->m_header.m_size);
                    std::cout << "body is now " << resp->m_body.size() << " bytes\n";
                    m_socket.read_some(asio::buffer(resp->m_body.data(), resp->m_body.size()), ec);
                } 
            }

            return resp;
        }

        protected:
            asio::io_context&       m_context;
            protqueue<message<T>>&  m_inbound;
            asio::ip::tcp::socket   m_socket;
            
            protqueue<message<T>>   m_outbound;
    };
}

#endif
