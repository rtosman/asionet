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
        client_interface()
        {
        }

        virtual ~client_interface()
        {
            disconnect();
        }

        [[nodiscard]] bool connect(const std::string& server, 
                                   const uint16_t port)
        {
            asio::error_code    ec;

            try 
            {
                asio::ip::tcp::resolver resolver(m_context);
                asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(server, std::to_string(port));

                m_connection = std::make_unique<connection<T>>(m_context, asio::ip::tcp::socket(m_context), m_msgs);

                m_connection->connect(endpoints);

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

        void send(message<T>& msg)
        {
            m_connection->send(msg);
        }

        [[nodiscard]] protqueue<owned_message<T>>& incoming()
        {
            return m_msgs;
        }

        private:
            asio::io_context                m_context;
            std::thread                     m_thrctxt;

            std::unique_ptr<connection<T>>  m_connection;
            protqueue<owned_message<T>>     m_msgs;
    };
}

#endif
