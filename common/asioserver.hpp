#ifndef _ASIOSERVER_HPP_INCLUDED
#define _ASIOSERVER_HPP_INCLUDED
#include "asionet.hpp"
#include "asioqueue.hpp"
#include "asiomsg.hpp"
#include "asiosession.hpp"
#include <list>
#include <functional>

// to debug define ASIO_ENABLE_HANDLER_TRACKING when building
namespace asionet
{
    template <typename T, bool Encrypt=true, bool Async=true>
    struct server_interface
    {
        using new_connection_notification_cb = std::function<bool(std::shared_ptr<session<T, Encrypt>>)>;
        using msg_ready_notification_cb = std::function<void()>;
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
                m_sessions.remove_if([&existing](std::shared_ptr<session<T, Encrypt>>& elem) 
                    {
                        return elem.get() == existing.get();
                    }
                );
            }
        }

        [[nodiscard]] protqueue<owned_message<T, Encrypt>>& incoming()
        {
            return m_msgs;
        }

    private:
        asio::io_context&                                        m_context;
        uint16_t                                                 m_port;
        new_connection_notification_cb                           m_connect_cb;
        msg_ready_notification_cb                                m_msg_ready_cb;
        disconnect_notification_cb                               m_disconnect_cb;
        protqueue<owned_message<T,Encrypt>>                      m_msgs;
        asio::ip::tcp::socket                                    m_socket;
        std::list<std::shared_ptr<session<T, Encrypt>>>          m_sessions;
        asio::ip::tcp::acceptor                                  m_acceptor;

        void remove_session(std::shared_ptr<session<T, Encrypt>> s)
        {
            for(auto i = m_sessions.begin(); i != m_sessions.end(); )
            {
                ((*i).get() == s.get())? i = m_sessions.erase(i):++i;
            }
        }

        void read_body_sync(std::shared_ptr<session<T, Encrypt>> s)
        {
            owned_message<T, Encrypt> t(s->get_hdr(), s);
            auto& owned_msg = m_msgs.create_inplace(std::move(t));

            if constexpr (Encrypt == true)
                owned_msg.m_msg.body().resize(asionet::crypto_align(owned_msg.m_msg.m_header.m_size));
            else
                owned_msg.m_msg.body().resize(owned_msg.m_msg.m_header.m_size);

            s->socket().read_some(asio::buffer(owned_msg.m_msg.body().data(),
                                               owned_msg.m_msg.body().size()
                                              )
                                 );
            if constexpr (Encrypt == true)
            {
                owned_msg.m_remote->decrypt(owned_msg);
            }

            m_msg_ready_cb();
        }

        void read_body_async(std::shared_ptr<session<T, Encrypt>> s)
        {
            owned_message<T, Encrypt> t(s->get_hdr(), s);
            auto& owned_msg = m_msgs.create_inplace(std::move(t));

            if constexpr (Encrypt == true)
                owned_msg.m_msg.body().resize(asionet::crypto_align(owned_msg.m_msg.m_header.m_size));
            else
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
                m_msg_ready_cb();
            }
        }

        void handle_read(owned_message<T>* owned_msg,
                         const asio::error_code& ec,
                         size_t bytes_transferred)
        {
            if(!ec)
            {
                if constexpr (Encrypt == true)
                {
                    owned_msg->m_remote->decrypt(owned_msg);
                }

                m_msg_ready_cb();
            }
            else
            {
                disconnect(owned_msg->m_remote);
            }            
        }

        void disconnect(std::shared_ptr<session<T, Encrypt>> s)
        {
            m_disconnect_cb(s);
            remove_session(s);
            // following line to be uncommented with c++20
            // std::erase_if(m_sessions, [&s](std::shared_ptr<session<T, Encrypt>> e) { return e.get() == s.get(); });
        }

        void prime()
        {
            if constexpr (Async == true)
                m_sessions.push_back(std::make_shared<session<T, Encrypt>>(m_context,
                                     std::bind(&server_interface::read_body_async,
                                               this,
                                               std::placeholders::_1),
                                     std::bind(&server_interface::disconnect,
                                               this,
                                               std::placeholders::_1)
                                                                 )
                                     );
            else
                m_sessions.push_back(std::make_shared<session<T, Encrypt>>(m_context,
                                     std::bind(&server_interface::read_body_sync,
                                               this,
                                               std::placeholders::_1),
                                     std::bind(&server_interface::disconnect,
                                               this,
                                               std::placeholders::_1)
                                                                 )
                                    );

            m_acceptor.async_accept(m_sessions.back()->socket(),
                std::bind(&server_interface::handle_accept,
                    this,
                    m_sessions.back(),
                    std::placeholders::_1
                )
            );
        }
    };
}

#endif
