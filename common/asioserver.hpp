#ifndef _ASIOSERVER_HPP_INCLUDED
#define _ASIOSERVER_HPP_INCLUDED
#include "asionet.hpp"
#include "asioqueue.hpp"
#include "asiomsg.hpp"
#include "asioutil.hpp"
#include "asiosession.hpp"
#include "asiobuiltin.hpp"
#include <list>
#include <map>
#include <functional>

using namespace std::chrono_literals;

// to debug define ASIO_ENABLE_HANDLER_TRACKING when building
namespace asionet
{
    template<typename T, bool Encrypt>
    void peak_sessions(stats& statistics, std::map<session<T, Encrypt>*, protqueue<owned_message<T, Encrypt>>>& msgs)
    {
        if (msgs.size() > statistics.peak_.sessions_)
        {
            // +1 because I don't want to add this code to the read_async call 
            // (for performance reasons) so this is called when a new authentication
            // has happened, so we know that current session has been added and 
            // we just added another session so there is n+1
            statistics.peak_.sessions_ = msgs.size() + 1;
        }
    }

    template<typename T, bool Encrypt>
    void peak_messages(stats& statistics, std::map<session<T, Encrypt>*, protqueue<owned_message<T, Encrypt>>>& msgs)
    {
        uint64_t m{ 0 };

        for (auto const& [key, q] : msgs)
        {
            m += q.size();
        }

        if (m > statistics.peak_.msgs_)
        {
            statistics.peak_.msgs_ = m;
        }
    }

    template <typename T, bool Encrypt=true>
    struct server_interface
    {
        using new_connection_notification_cb = std::function<bool(std::shared_ptr<session<T, Encrypt>>)>;
        using msg_ready_notification_cb = std::function<void(protqueue<owned_message<T, Encrypt>>&)>;
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
                                m_read(ctxt), m_write(ctxt),
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

        asionet::stats& statistics()
        {
            return m_stats;
        }

        template <typename F1>
        void send(std::shared_ptr<session<T, Encrypt>> s, std::shared_ptr<message<T>> msg, F1 cb)
        {
            s->send(msg, [cb]() {cb();});
        }

    private:
        asio::io_context&                                                   m_context;
        asio::io_context::strand                                            m_read;
        asio::io_context::strand                                            m_write; 
        uint16_t                                                            m_port;
        new_connection_notification_cb                                      m_connect_cb;
        msg_ready_notification_cb                                           m_msg_ready_cb;
        disconnect_notification_cb                                          m_disconnect_cb;
        std::map<session<T, Encrypt>*, protqueue<owned_message<T,Encrypt>>> m_msgs;
        asio::ip::tcp::socket                                               m_socket;
        asio::ip::tcp::acceptor                                             m_acceptor;
        std::list<std::shared_ptr<session<T, Encrypt>>>                     m_sessions;
        std::mutex                                                          m_msg_mutex;
        stats                                                               m_stats{{0,0},{0,0}};

        void authenticate(std::shared_ptr<session<T, Encrypt>> s,
                          const std::chrono::seconds& tmout)
        {
            if constexpr (Encrypt == true)
            {
                // message 0 is reserved for authentication
                // the IV field in the header is used to supply
                // the random data for the challenge
                std::shared_ptr<message<T>> auth = std::make_shared<message<T>>();
                s->current_challenge(init_challenge(&auth->m_header.m_iv[0], 
                                                    sizeof(auth->m_header.m_iv)
                                                   )
                                           ); 

                s->send(auth, // v--- must make a copy of tmout at this point
                        [s, tmout, this]() -> void
                            {
                                message_header<T> resp;
                                auto [read_err, timeout] = read_with_timeout(s->socket(),
                                                                             asio::buffer(&resp,
                                                                                          sizeof resp),
                                                                             tmout
                                                                            );

                                Botan::secure_vector<uint8_t> foo(&resp.m_iv[0], &resp.m_iv[16]);
                                if (!timeout && (read_err && !read_err.value()))
                                {
                                    if (verify_response<T>(s, 
                                                        reinterpret_cast<uint8_t*>(&resp.m_iv),
                                                        s->current_challenge()) == 0)
                                    {
                                        s->start();
                                        std::scoped_lock<std::mutex> lock(m_msg_mutex);
                                        peak_sessions<T,Encrypt>(m_stats, m_msgs);
                                        m_sessions.push_back(s);
                                    }
                                    else 
                                    {
                                        m_disconnect_cb(s);
                                    }
                                }
                                else if(read_err && read_err.value())
                                {
                                    m_disconnect_cb(s);
                                } 
                                else if(timeout)
                                {
                                    m_disconnect_cb(s);
                                }
                            }
                );
            }
            else
            {
                s->start();
                std::scoped_lock<std::mutex> lock(m_msg_mutex);
                peak_sessions<T,Encrypt>(m_stats, m_msgs);
            }
        }

        void handle_accept(std::shared_ptr<session<T, Encrypt>> s,
                           const std::chrono::seconds& tmout,
                           const asio::error_code ec)
        {
            if (!ec)
            {
                if (m_connect_cb(s))
                {
                    authenticate(s, tmout); 
                }
                else
                {
                    s->socket().close();
                    remove_session(s);
                }
                // must re-prime any time that a session has been either consumed (start)
                // or destroyed (remove_session)
                prime();
            }
            else
            {
                s->socket().close();
                remove_session(s);
            }
        }

        void remove_session(std::shared_ptr<session<T, Encrypt>> s)
        {
            std::this_thread::sleep_for(100ms); // give time for all I/O to fail on closed socket
            std::scoped_lock<std::mutex> lock(m_msg_mutex);
            auto msgit = m_msgs.find(s.get());
            if(msgit != m_msgs.end())
            {
                m_msgs.erase(msgit);
            }
            m_sessions.remove(s);
        }

        void read_body(std::shared_ptr<session<T, Encrypt>> s)
        {
            std::scoped_lock<std::mutex> lock(m_msg_mutex);
            auto& owned_msg = m_msgs[s.get()].create_inplace(s->get_hdr(), s);

            if constexpr (Encrypt == true) 
            {
                owned_msg.m_msg.body().resize(asionet::crypto_align(owned_msg.m_msg.m_header.m_size));
                assert(owned_msg.m_msg.body().size() >= AESBlockSize);
            } else
                owned_msg.m_msg.body().resize(owned_msg.m_msg.m_header.m_size);

            // if the body size is 0, then handle_read will be called "immediately" while still 
            // preserving the I/O pump
            asio::async_read(s->socket(),
                                asio::buffer(owned_msg.m_msg.body().data(),
                                            owned_msg.m_msg.body().size()),
                                asio::bind_executor(m_read, 
                                                    std::bind(&server_interface::handle_read,
                                                                this,
                                                                &owned_msg,
                                                                std::placeholders::_1,
                                                                std::placeholders::_2
                                                            )
                                )
            );
        }

        void handle_read(owned_message<T, Encrypt>* owned_msg,
                         const asio::error_code& ec,
                         size_t bytes_transferred)
        {
            std::scoped_lock<std::mutex> lock(m_msg_mutex);
            peak_messages<T,Encrypt>(m_stats, m_msgs);
            ++m_stats.count_.msgs_rx_good_;

            if (!ec)
            {
                if constexpr (Encrypt == true)
                {
                    if (bytes_transferred)
                    {
                        assert(bytes_transferred >= AESBlockSize);
                        owned_msg->m_remote->decrypt(owned_msg->m_msg);
                    }
                }

                std::shared_ptr<session<T, Encrypt>> s = owned_msg->m_remote;
                m_msg_ready_cb(m_msgs[s.get()]);
                s->start(); 
            }
            else
            {
                ++m_stats.count_.msgs_rx_bad_;
                disconnect(owned_msg->m_remote);
            }            
        }

        void disconnect(std::shared_ptr<session<T, Encrypt>> s)
        {
            s->socket().close();
            m_disconnect_cb(s);
            remove_session(s);
        }

        void prime()
        {
            std::shared_ptr<asionet::session<T, Encrypt>> s = 
                      std::make_shared<session<T, Encrypt>>(m_context, 
                                                            m_read, 
                                                            m_write,
                                                            std::bind(&server_interface::read_body,
                                                                    this,
                                                                    std::placeholders::_1),
                                                            std::bind(&server_interface::disconnect,
                                                                    this,
                                                                    std::placeholders::_1)
                                                            );

            m_acceptor.async_accept(s->socket(), 
                asio::bind_executor(m_read, std::bind(&server_interface::handle_accept,
                                                        this,
                                                        s,
                                                        10s,
                                                        std::placeholders::_1
                                                     )
                )
            );
        }
    };
}

#endif
