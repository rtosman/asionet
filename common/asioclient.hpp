#ifndef _ASIOCLIENT_HPP_INCLUDED
#define _ASIOCLIENT_HPP_INCLUDED
#include <thread>
#include "asionet.hpp"
#include "asioqueue.hpp"
#include "asiomsg.hpp"
#include "asiosession.hpp"
#include "one.hpp"

namespace asionet 
{
    template <typename T, bool Encrypt=true>
    struct client_interface
    {
        using sess = std::shared_ptr<asionet::session<T>>;
        using decrypt_type = std::unique_ptr<Botan::Cipher_Mode>;
        using msg_ready_notification_cb = std::function<void()>;
        using disconnect_notification_cb = std::function<void(std::shared_ptr<session<T>>)>;

        template <typename F1,
                  typename F2>
        client_interface(asio::io_context& ctxt,
                         F1 msg_ready_cb,
                         F2 disconnect_cb,
                        std::vector<uint8_t> key = Botan::hex_decode("2B7E151628AED2A6ABF7158809CF4F3C")):
            m_context(ctxt),
            m_msg_ready_cb(msg_ready_cb),
            m_disconnect_cb(disconnect_cb),
            m_dec(Botan::Cipher_Mode::create("AES-128/CBC/PKCS7", Botan::DECRYPTION))
        {
            m_dec->set_key(key);
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

                if constexpr (Encrypt == true)
                    m_session = std::make_shared<session<T>>(m_context,
                                        std::bind(&client_interface::read_body_async,
                                                this,
                                                std::placeholders::_1),
                                        std::bind(&client_interface::disconnect,
                                                this)
                                );
                else
                    m_session = std::make_shared<session<T, false>>(m_context,
                                     std::bind(&client_interface::read_body_sync,
                                               this,
                                               std::placeholders::_1),
                                     std::bind(&client_interface::disconnect,
                                               this,
                                               std::placeholders::_1)
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
            if(is_connected())
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
            m_session->write(msg, [cb](sess s) 
                {
                    cb();
                });
        }

        [[nodiscard]] protqueue<owned_message<T>>& incoming()
        {
            return m_msgs;
        }

    private:
        asio::io_context&                                       m_context;
        msg_ready_notification_cb                               m_msg_ready_cb;
        disconnect_notification_cb                              m_disconnect_cb;
        std::thread                                             m_thrctxt;

        std::shared_ptr<session<T>>                             m_session;
        protqueue<owned_message<T>>                             m_msgs;
        decrypt_type                                            m_dec;

        uint32_t crypto_align(uint32_t size)
        {
            return size + (16-(size % 16));
        }

        void read_body_sync(std::shared_ptr<session<T>> s)
        {
            owned_message<message<T>> t(s->get_hdr(), s);
            auto& owned_msg = m_msgs.create_inplace(t);
            if constexpr (Encrypt == true)
                owned_msg.m_msg.body().resize(crypto_align(owned_msg.m_msg.m_header.m_size));
            else
                owned_msg.m_msg.body().resize(owned_msg.m_msg.m_header.m_size);

            s->socket().read_some(asio::buffer(owned_msg.m_msg.body().data(),
                                               owned_msg.m_msg.body().size()
                                              )
                                 );
            if constexpr (Encrypt == true)
            {
                Botan::secure_vector<uint8_t> iv(&owned_msg.m_msg.m_header.m_iv[0],
                                                 &owned_msg.m_msg.m_header.m_iv[16]);
                m_dec->start(iv);
                m_dec->finish(owned_msg.m_msg.body());
            }
            m_msg_ready_cb();
        }

        void read_body_async(std::shared_ptr<session<T>> s)
        {
            owned_message t(s->get_hdr(), s);
            auto& owned_msg=m_msgs.create_inplace(std::move(t));
            if constexpr (Encrypt == true)
                owned_msg.m_msg.body().resize(crypto_align(owned_msg.m_msg.m_header.m_size));
            else
                owned_msg.m_msg.body().resize(owned_msg.m_msg.m_header.m_size);
            
            if (s->get_hdr().m_size) {
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
                m_msg_ready_cb();
            }
        }

        void handle_read(owned_message<T>* owned_msg,
                         const asio::error_code& ec,
                         size_t bytes_transferred)
        {
            if (!ec)
            {
                if constexpr (Encrypt == true)
                {
                    Botan::secure_vector<uint8_t> iv(&owned_msg->m_msg.m_header.m_iv[0],
                                                     &owned_msg->m_msg.m_header.m_iv[16]);
                    m_dec->start(iv);
                    m_dec->finish(owned_msg->m_msg.body());
                }

                m_msg_ready_cb();
            }
            else
            {
                disconnect();
            }
        }
    };
}
#endif
