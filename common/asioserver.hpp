#ifndef _ASIOSERVER_HPP_INCLUDED
#define _ASIOSERVER_HPP_INCLUDED
#include "asionet.hpp"
#include "asioqueue.hpp"
#include "asiomsg.hpp"
#include "asioconnection.hpp"
#include <list>
#include <functional>

// to debug define ASIO_ENABLE_HANDLER_TRACKING when building
namespace asionet 
{

    template <typename T>
    struct session: public std::enable_shared_from_this<session<T>>
    {
        using msg_cb = std::function<void(std::shared_ptr<session<T>>)>;
        using wr_cb = std::function<void(std::shared_ptr<session<T>>)>;
        using err_cb = std::function<void(std::shared_ptr<session<T>>)>;
        using enable_shared = std::enable_shared_from_this<session<T>>;

        session(asio::io_service& ios, msg_cb mcb, err_cb ecb)
            : m_socket(ios), m_mcb(mcb), m_ecb(ecb)
        {
        }

        ~session()
        {
        }

        asio::ip::tcp::socket& socket()
        {
            return m_socket;
        }

        message_header<T>& get_hdr()
        {
            return m_data;
        }

        void start()
        {
            m_socket.async_read_some(asio::buffer((uint8_t*)&m_data, 
                                                  sizeof(m_data)
                                                 ),
                                     std::bind(&session::handle_read, 
                                               enable_shared::shared_from_this(),
                                               std::placeholders::_1,
                                               std::placeholders::_2
                                        )
                                    );
        }

        void write(message<T>& msg, wr_cb cb)
        {
            asio::async_write(m_socket,
                              asio::buffer((uint8_t*)&msg, sizeof(msg.m_header)),
                              std::bind(&session::handle_body,
                                        enable_shared::shared_from_this(),
                                        msg.m_body.data(),
                                        msg.m_body.size(),
                                        cb,
                                        std::placeholders::_1
                                        )
                            );

        }

        private:
        void handle_read(const asio::error_code& ec,
                         size_t bytes_transferred)
        {
            if (!ec)
            {
                m_mcb(enable_shared::shared_from_this());
                start(); // we've handled one message, start again
            }
            else
            {
                m_ecb(enable_shared::shared_from_this());
            }
        }

        void handle_body(uint8_t* data, size_t len, wr_cb cb, 
                         const asio::error_code& error)
        {
            if (!error)
            {
                asio::async_write(m_socket,
                                asio::buffer(data,len), 
                                std::bind(&session::handle_write_completion,
                                            enable_shared::shared_from_this(),
                                            cb,
                                            std::placeholders::_1
                                            )
                                );
            }
        }

        void handle_write_completion(wr_cb wcb, const asio::error_code& error)
        {
            if (!error)
            {
                wcb(std::enable_shared_from_this<session<T>>::shared_from_this());
            }
        }

    private:
        asio::ip::tcp::socket       m_socket;
        msg_cb                      m_mcb;
        err_cb                      m_ecb;
        asionet::message_header<T>  m_data;
    };

    template <typename T, bool Async=true>
    struct server_interface
    {
        using new_connection_notification_cb = std::function<bool(std::shared_ptr<asionet::session<T>>)>;
        using msg_ready_notification_cb = std::function<void()>;
        using disconnect_notification_cb = std::function<void(std::shared_ptr<asionet::session<T>>)>;

        template <typename F1, 
                  typename F2, 
                  typename F3>
        server_interface(asio::io_service& ios, 
                         uint16_t port,
                         F1 connect_cb,
                         F2 msg_ready_cb,
                         F3 disconnect_cb):
                                m_ios(ios),
                                m_port(port),
                                m_connect_cb(connect_cb),
                                m_msg_ready_cb(msg_ready_cb),
                                m_disconnect_cb(disconnect_cb),
                                m_socket(m_ios),
                                m_acceptor(m_ios, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), m_port))
        {
            // prime the pump
            prime();
        }

        virtual ~server_interface()
        {
        }

        void handle_accept(std::shared_ptr<session<T>> existing,
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
                m_sessions.remove_if([&existing](std::shared_ptr<session<T>>& elem) 
                    {
                        return elem.get() == existing.get();
                    }
                );
            }
        }

        [[nodiscard]] protqueue<owned_message<T>>& incoming()
        {
            return m_msgs;
        }

    private:
        asio::io_service&                           m_ios;
        uint16_t                                    m_port;
        new_connection_notification_cb              m_connect_cb;
        msg_ready_notification_cb                   m_msg_ready_cb;
        disconnect_notification_cb                  m_disconnect_cb;
        protqueue<owned_message<T>>                 m_msgs;
        asio::ip::tcp::socket                       m_socket;
        std::list<std::shared_ptr<session<T>>>      m_sessions;
        asio::ip::tcp::acceptor                     m_acceptor;

        void remove_session(std::shared_ptr<session<T>> s)
        {
            for(auto i = m_sessions.begin(); i != m_sessions.end(); )
            {
                ((*i).get() == s.get())? i = m_sessions.erase(i):++i;
            }
        }

        void read_body_sync(std::shared_ptr<session<T>> s)
        {
            asionet::owned_message owned_msg(s->get_hdr(), s);
            owned_msg.m_msg.body().resize(owned_msg.m_msg.m_header.m_size);
            s->socket().read_some(asio::buffer(owned_msg.m_msg.body().data(),
                                               owned_msg.m_msg.body().size()
                                              )
                                 );

            m_msgs.push_back(owned_msg);
            m_msg_ready_cb();
        }

        void read_body_async(std::shared_ptr<session<T>> s)
        {
            auto owned_msg = std::make_shared<owned_message<T>>(s->get_hdr(), s); 
            owned_msg->m_msg.body().resize(owned_msg->m_msg.m_header.m_size);
            s->socket().async_read_some(asio::buffer(owned_msg->m_msg.body().data(),
                                                     owned_msg->m_msg.body().size()),
                                        std::bind(&server_interface::handle_read,
                                                  this,
                                                  owned_msg,
                                                  std::placeholders::_1,
                                                  std::placeholders::_2
                                                )
                                       );
        }

        void handle_read(std::shared_ptr<owned_message<T>> owned_msg,
                         const asio::error_code& ec,
                         size_t bytes_transferred)
        {
            if(!ec)
            {
                m_msgs.push_back(*owned_msg);
                m_msg_ready_cb();
            }
            else
            {
                disconnect(owned_msg->m_remote);
            }            
        }

        void disconnect(std::shared_ptr<session<T>> s)
        {
            m_disconnect_cb(s);
            remove_session(s);
            // following line to be uncommented with c++20
            // std::erase_if(m_sessions, [&s](std::shared_ptr<session<T>> e) { return e.get() == s.get(); });
        }

        void prime()
        {
            if constexpr (Async == true)
                m_sessions.push_back(std::make_shared<session<T>>(m_ios,
                                     std::bind(&server_interface::read_body_async,
                                               this,
                                               std::placeholders::_1),
                                     std::bind(&server_interface::disconnect,
                                               this,
                                               std::placeholders::_1)
                                                                 )
                                     );
            else
                m_sessions.push_back(std::make_shared<session<T>>(m_ios,
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
