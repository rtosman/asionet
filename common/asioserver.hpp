#ifndef _ASIOSERVER_HPP_INCLUDED
#define _ASIOSERVER_HPP_INCLUDED
#include "asionet.hpp"
#include "asioqueue.hpp"
#include "asiomsg.hpp"
#include "asiosession.hpp"
#include <list>
#include <map>
#include <functional>

// to debug define ASIO_ENABLE_HANDLER_TRACKING when building
namespace asionet
{
    template <typename T, bool Encrypt=true, bool Async=true>
    struct server_interface
    {
        using new_connection_notification_cb = std::function<bool(std::shared_ptr<session<T, Encrypt>>)>;
        using msg_ready_notification_cb = std::function<void(protqueue<owned_message<T,Encrypt>>&)>;
        using disconnect_notification_cb = std::function<void(std::shared_ptr<session<T, Encrypt>>)>;

        template <typename F1, 
                  typename F2, 
                  typename F3>
        server_interface(asio::io_context& ctxt, 
                         uint16_t port,
                         F1 connect_cb,
                         F2 msg_ready_cb,
                         F3 disconnect_cb,
                         std::vector<uint8_t> key = Botan::hex_decode("2B7E151628AED2A6ABF7158809CF4F3C")):
                                m_context(ctxt),
                                m_port(port),
                                m_connect_cb(connect_cb),
                                m_msg_ready_cb(msg_ready_cb),
                                m_disconnect_cb(disconnect_cb),
                                m_socket(m_context),
                                m_acceptor(m_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), m_port))
        {
            // prime the pump
            prime();
        }

        virtual ~server_interface()
        {
        }

        void handle_accept(std::shared_ptr<session<T, Encrypt>> existing,
                           const asio::error_code ec)
        {
            if (!ec)
            {
                if (m_connect_cb(existing))
                {
                    peak_sessions();
                    existing->start();
                }
                else
                {
                    remove_session(existing);
                }
                // must re-prime any time that a session has been either consumed (start)
                // or destroyed (remove_session)
                prime();
            }
            else
            {
                remove_session(existing);
            }
        }

        asionet::stats& statistics()
        {
            return m_stats;
        }

    private:
        asio::io_context&                                                   m_context;
        uint16_t                                                            m_port;
        new_connection_notification_cb                                      m_connect_cb;
        msg_ready_notification_cb                                           m_msg_ready_cb;
        disconnect_notification_cb                                          m_disconnect_cb;
        std::map<session<T, Encrypt>*, protqueue<owned_message<T,Encrypt>>> m_msgs;
        asio::ip::tcp::socket                                               m_socket;
        asio::ip::tcp::acceptor                                             m_acceptor;
        stats                                                               m_stats;

        void remove_session(std::shared_ptr<session<T, Encrypt>> s)
        {
            auto it = m_msgs.find(s.get());
            m_msgs.erase(it);                
        }

        void peak_sessions()
        {
            if(m_msgs.size() > m_stats.peak_.sessions_)
            {
                m_stats.peak_.sessions_ = m_msgs.size();
            }
        }

        void peak_messages()
        {
            uint64_t msgs{0};

            for(auto const& [key, q]: m_msgs)
            {
                msgs += q.size();
            }

            if(msgs > m_stats.peak_.msgs_)
            {
                m_stats.peak_.msgs_ = msgs;
            }
        }

        void read_body_sync(std::shared_ptr<session<T, Encrypt>> s)
        {
            auto& owned_msg = m_msgs[s.get()].create_inplace(s->get_hdr(), s);
            
            peak_messages();

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
                s->socket().async_read_some(asio::buffer(owned_msg.m_msg.body().data(),
                                                         owned_msg.m_msg.body().size()),
                    std::bind(&server_interface::handle_read,
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
            peak_messages();
            ++m_stats.count_.msgs_rx_good_;

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
                ++m_stats.count_.msgs_rx_bad_;
                disconnect(owned_msg->m_remote);
            }            
        }

        void disconnect(std::shared_ptr<session<T, Encrypt>> s)
        {
            m_disconnect_cb(s);
            remove_session(s);
        }

        void prime()
        {
            std::shared_ptr<asionet::session<T, Encrypt>> s;

            if constexpr (Async == true)
                s = std::make_shared<session<T, Encrypt>>(m_context,
                                     std::bind(&server_interface::read_body_async,
                                               this,
                                               std::placeholders::_1),
                                     std::bind(&server_interface::disconnect,
                                               this,
                                               std::placeholders::_1)
                                                         );
            else
                s = std::make_shared<session<T, Encrypt>>(m_context,
                                     std::bind(&server_interface::read_body_sync,
                                               this,
                                               std::placeholders::_1),
                                     std::bind(&server_interface::disconnect,
                                               this,
                                               std::placeholders::_1)
                                                         );

            m_acceptor.async_accept(s->socket(),
                std::bind(&server_interface::handle_accept,
                    this,
                    s,
                    std::placeholders::_1
                )
            );
        }
    };
}

#endif
