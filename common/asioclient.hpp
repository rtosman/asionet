#ifndef _ASIOCLIENT_HPP_INCLUDED
#define _ASIOCLIENT_HPP_INCLUDED
#include "asionet.hpp"
#include "asioqueue.hpp"
#include "asiomsg.hpp"
#include "asioutil.hpp"
#include "asiosession.hpp"
#include "one.hpp"
#include <map>
#include <cassert>

using namespace std::chrono_literals;

namespace asionet 
{
    template <typename T, bool Encrypt=true>
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
                                   const uint16_t port,
                                   const std::chrono::seconds& tmout)
        {
            asio::error_code    ec;

            try 
            {
                asio::ip::tcp::resolver resolver(m_context);
                asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(server, std::to_string(port));

                m_session = std::make_shared<session<T, Encrypt>>(m_context,
                                    std::bind(&client_interface::read_body,
                                            this,
                                            std::placeholders::_1),
                                    std::bind(&client_interface::disconnect,
                                                this)
                            );

                if constexpr (Encrypt == true)
                {
                    m_session->connect(endpoints, [&tmout, this]()
                        {
                            message<T> chlng;

                            read_with_timeout(m_context,
                                              m_session->socket(),
                                              asio::buffer(&chlng.m_header, sizeof chlng.m_header),
                                              tmout
                            );

                            m_session->encrypt(reinterpret_cast<uint8_t*>(&chlng.m_header.m_iv),
                                sizeof chlng.m_header.m_iv);
                            m_session->send(chlng, []() {});
                            m_session->start();
                        }
                    );
                }
                else
                {
                    m_session->connect(endpoints, [this]() { m_session->start(); });
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
            m_session->send(msg, [cb]() {cb();});
        }

    private:
        asio::io_context&                                                   m_context;
        msg_ready_notification_cb                                           m_msg_ready_cb;
        disconnect_notification_cb                                          m_disconnect_cb;
        std::thread                                                         m_thrctxt;
        std::shared_ptr<session<T, Encrypt>>                                m_session;
        std::map<session<T, Encrypt>*, protqueue<owned_message<T,Encrypt>>> m_msgs;
        std::condition_variable                                             m_cv;
        std::mutex                                                          m_mutex;
        bool                                                                m_send_pending{ false };

        void read_body(std::shared_ptr<session<T, Encrypt>> s)
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
                asio::async_read(s->socket(),
                                 asio::buffer(owned_msg.m_msg.body().data(),
                                              owned_msg.m_msg.body().size()
                                             ),
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
