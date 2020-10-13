#ifndef _ASIOCLIENT_HPP_INCLUDED
#define _ASIOCLIENT_HPP_INCLUDED
#include "asionet.hpp"
#include "asioqueue.hpp"
#include "asiomsg.hpp"
#include "asioconnection.hpp"

namespace asionet 
{
    template <typename T>
    struct client_interface
    {
        client_interface():m_socket(m_context)
        {

        }

        virtual ~client_interface()
        {
            disconnect();
        }

        [[nodiscard]] bool connect(const std::string& server, 
                                   const uint16_t port)
        {
            asio::io_service    ios;
            asio::error_code    ec;

            try 
            {
                m_connection = std::make_unique<connection<T>>(m_context, m_msgs);

                asio::ip::tcp::resolver::query resolver_query(server,
                                                              std::to_string(port),
                                                              asio::ip::tcp::resolver::query::numeric_service
                                                             );

                asio::ip::tcp::resolver resolver(ios);

                m_endpoints = resolver.resolve(resolver_query, ec);

                if (ec) {
                    std::cerr << "Failed to resolve a DNS name."
                        << "Error code = " << ec.value() 
                        << ". Message = " << ec.message();
                    return false;
                }

                if (!m_connection->connect(m_endpoints->endpoint()))
                {
                    return false;

                }
                m_thrctxt = std::thread([this]() { m_context.run(); });
            }
            catch(std::exception& e)
            {
                std::cerr << "Client exception: " << e.what() << "\n";
                return false;
            }

            return true; 
        }

        void disconnect()
        {
            if(is_connected())
            {
                m_connection->disconnect();
            }

            m_context.stop();
            if(m_thrctxt.joinable())
            {
                m_thrctxt.join();
            }

            m_connection.release();
        }

        [[nodiscard]] bool is_connected()
        {
            if(m_connection)
                return m_connection->is_connected();
            else
                return false;
        }

        [[nodiscard]] protqueue<message<T>>& incoming()
        {
            return m_msgs;
        }

        protected:
            asio::io_context                m_context;
            std::thread                     m_thrctxt;
            asio::ip::tcp::socket           m_socket;
            std::unique_ptr<connection<T>>  m_connection;

        private:
            protqueue<message<T>>               m_msgs;
            asio::ip::tcp::resolver::iterator   m_endpoints; 
    };
}

#endif
