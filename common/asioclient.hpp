#ifndef _ASIOCLIENT_HPP_INCLUDED
#define _ASIOCLIENT_HPP_INCLUDED
#include <thread>
#include "asionet.hpp"
#include "asioqueue.hpp"
#include "asiomsg.hpp"
#include "asiosession.hpp"
#include "one.hpp"
#include <map>
#include <cassert>

namespace asionet 
{
    template <typename T, bool Encrypt=true, bool Async=true>
    struct client_interface
    {
        using sess_type = std::shared_ptr<asionet::session<T, Encrypt>>;
        using msg_ready_notification_cb = std::function<void(protqueue<owned_message<T,Encrypt>>&)>;
        using disconnect_notification_cb = std::function<void(std::shared_ptr<session<T, Encrypt>>)>;

        template <typename F1,
                  typename F2>
        client_interface(asio::io_context& ctxt,
                         F1 msg_ready_cb,
                         F2 disconnect_cb):
            m_context(ctxt),
            m_msg_ready_cb(msg_ready_cb),
            m_disconnect_cb(disconnect_cb)
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

                if constexpr (Async == true)
                    m_session = std::make_shared<session<T, Encrypt>>(m_context,
                                        std::bind(&client_interface::read_body_async,
                                                this,
                                                std::placeholders::_1),
                                        std::bind(&client_interface::disconnect,
                                                  this)
                                );
                else
                    m_session = std::make_shared<session<T, Encrypt>>(m_context,
                                     std::bind(&client_interface::read_body_sync,
                                               this,
                                               std::placeholders::_1),
                                     std::bind(&client_interface::disconnect,
                                               this)
                                );

                m_session->connect(endpoints);

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
            if(is_connected() && m_session)
            {
                m_session->disconnect();
            }

            m_context.stop();
            if(m_thrctxt.joinable())
            {
                m_thrctxt.join();
            }
        }

        [[nodiscard]] bool is_connected()
        {
            if(m_session)
                return m_session->is_connected();
            else
                return false;
        }

        template <typename F1>
        void send(message<T>& msg, F1 cb)
        {
            m_session->write(msg, [cb](sess_type s) 
                {
                    cb();
                });
        }

    private:
        asio::io_context&                                                   m_context;
        msg_ready_notification_cb                                           m_msg_ready_cb;
        disconnect_notification_cb                                          m_disconnect_cb;
        std::thread                                                         m_thrctxt;
        std::shared_ptr<session<T, Encrypt>>                                m_session;
        std::map<session<T, Encrypt>*, protqueue<owned_message<T,Encrypt>>> m_msgs;
        std::mutex                                                          m_mutex;

        void read_body_sync(std::shared_ptr<session<T, Encrypt>> s)
        {
            auto& owned_msg = m_msgs[s.get()].create_inplace(s->get_hdr(), s);

            if constexpr (Encrypt == true)
            {
                owned_msg.m_msg.body().resize(asionet::crypto_align(owned_msg.m_msg.m_header.m_size));
                assert(owned_msg.m_msg.body().size() >= AESBlockSize);
            } else
                owned_msg.m_msg.body().resize(owned_msg.m_msg.m_header.m_size);

            s->socket().read_some(asio::buffer(owned_msg.m_msg.body().data(),
                                               owned_msg.m_msg.body().size()
                                              )
                                 );
            if constexpr (Encrypt == true)
            {
                owned_msg.m_remote->decrypt(owned_msg.m_msg);
            }

            m_msg_ready_cb(m_msgs[s->get()]);
            s->start();
        }

        void read_body_async(std::shared_ptr<session<T, Encrypt>> s)
        {
            auto& owned_msg = m_msgs[s.get()].create_inplace(s->get_hdr(), s);

            if constexpr (Encrypt == true) 
            {
                owned_msg.m_msg.body().resize(asionet::crypto_align(owned_msg.m_msg.m_header.m_size));
                assert(owned_msg.m_msg.body().size() >= AESBlockSize);
            } else
                owned_msg.m_msg.body().resize(owned_msg.m_msg.m_header.m_size);

            if (s->get_hdr().m_size)
            {
                // if(m_msgs.size() > 1)
                //     std::cout << "enqueuing async read body\n";
                s->socket().async_read_some(asio::buffer(owned_msg.m_msg.body().data(),
                                                         owned_msg.m_msg.body().size()),
                    std::bind(&client_interface::handle_read,
                        this,
                        &owned_msg,
                        std::placeholders::_1,
                        std::placeholders::_2
                    )
                );
            }
            else
            {
                m_msg_ready_cb(m_msgs[s.get()]);
                s->start();
            }
        }
        
        void handle_read(owned_message<T, Encrypt>* owned_msg,
                         const asio::error_code& ec,
                         size_t bytes_transferred)
        {
            if (!ec)
            {
                if constexpr (Encrypt == true)
                {
                    assert(bytes_transferred >= AESBlockSize);
                    owned_msg->m_remote->decrypt(owned_msg->m_msg);
                }

                std::shared_ptr<session<T, Encrypt>> s = owned_msg->m_remote;
                m_msg_ready_cb(m_msgs[s.get()]);
                s->start(); // we've handled the message, start again
            }
            else
            {
                std::cout << "handle_read encountered a read error: " << ec << "\n";
                disconnect();
            }            
        }
    };
}
#endif
