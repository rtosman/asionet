#ifndef _ASIOSERVER_HPP_INCLUDED
#define _ASIOSERVER_HPP_INCLUDED
#include "asionet.hpp"
#include "asioqueue.hpp"
#include "asiomsg.hpp"
#include "asioutil.hpp"
#include "asiosession.hpp"
#include <list>
#include <map>
#include <functional>

using namespace std::chrono_literals;

// to debug define ASIO_ENABLE_HANDLER_TRACKING when building
namespace asionet
{
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
                           const std::chrono::seconds& tmout,
                           const asio::error_code ec)
        {
            if (!ec)
            {
                if (m_connect_cb(existing))
                {
                    peak_sessions();
                    authenticate(existing, tmout); 
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

        template <typename F1>
        void send(std::shared_ptr<session<T, Encrypt>> s, message<T>& msg, F1 cb)
        {
            s->send(msg, [cb]() {cb();});
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
        std::tuple<std::shared_ptr<uint8_t>, size_t>                        m_cur_challenge;
        bool                                                                m_valid{false};

        std::tuple<std::shared_ptr<uint8_t>, size_t> init_challenge(uint8_t* space, size_t amount)
        {
            std::shared_ptr<uint8_t> copy(new uint8_t[amount]);

            Botan::AutoSeeded_RNG rng;
            Botan::secure_vector<uint8_t> random_data = rng.random_vec(amount);
            std::memcpy(space, random_data.data(), random_data.size());
            std::memcpy(copy.get(), space, random_data.size());
            return std::make_tuple(copy, random_data.size());
        }

        unsigned char verify_response(std::shared_ptr<session<T, Encrypt>> s, 
                                      uint8_t* resp, 
                                      std::tuple<std::shared_ptr<uint8_t>, size_t>& answer)
        {
            s->decrypt(&resp[0],std::get<1>(answer));
            return memcmp(&resp[0], std::get<0>(answer).get(), std::get<1>(answer));
        }
        
        void authenticate(std::shared_ptr<session<T, Encrypt>> existing,
                          const std::chrono::seconds& tmout)
        {
            if constexpr (Encrypt == true)
            {
                // message 0 is reserved for authentication
                // the IV field in the header is used to supply
                // the random data for the challenge
                std::shared_ptr<message<T>> auth(new message<T>());
                m_cur_challenge = init_challenge(&auth->m_header.m_iv[0], 
                                                 sizeof(auth->m_header.m_iv)); 
                existing->send(*auth,
                    [&existing, &tmout, this]() -> void
                    {
                        message_header<T> resp;
                        auto [read_err, timeout] = read_with_timeout(m_context,
                                          existing->socket(),
                                          asio::buffer(&resp,
                                                       sizeof resp),
                                          tmout
                                         );
                        if (!timeout && (read_err && !read_err.value()))
                        {
                            if (verify_response(existing, 
                                                reinterpret_cast<uint8_t*>(&resp.m_iv),
                                                m_cur_challenge) == 0)
                            {
                                m_valid = true;
                                existing->start();
                            }
                        }
                    }
                );
            }
            else
            {
                existing->start();
            }
        }

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
                asio::async_read(s->socket(),
                                 asio::buffer(owned_msg.m_msg.body().data(),
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
            m_disconnect_cb(s);
            remove_session(s);
        }

        void prime()
        {
            std::shared_ptr<asionet::session<T, Encrypt>> s;

            s = std::make_shared<session<T, Encrypt>>(m_context,
                                    std::bind(&server_interface::read_body,
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
                    10s,
                    std::placeholders::_1
                )
            );
        }
    };
}

#endif
