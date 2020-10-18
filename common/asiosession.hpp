#ifndef _ASIOSESSION_HPP_INCLUDED
#define _ASIOSESSION_HPP_INCLUDED
#include "asionet.hpp"
#include "asioqueue.hpp"
#include "asiomsg.hpp"
#include <botan/cipher_mode.h>
#include <botan/hex.h>
#include <cstdint>

namespace asionet 
{
    template <uint8_t BlockSize = 16>
    uint32_t crypto_align(uint32_t size)
    {
        return size + (BlockSize - (size % BlockSize));
    }

    template <typename T, bool Encrypt>
    struct session: public std::enable_shared_from_this<session<T, Encrypt>>
    {
        using msg_cb = std::function<void(std::shared_ptr<session<T, Encrypt>>)>;
        using wr_cb = std::function<void(std::shared_ptr<session<T, Encrypt>>)>;
        using err_cb = std::function<void(std::shared_ptr<session<T, Encrypt>>)>;
        using enable_shared = std::enable_shared_from_this<session<T, Encrypt>>;
        using encrypt_type = std::unique_ptr<Botan::Cipher_Mode>;
        using decrypt_type = std::unique_ptr<Botan::Cipher_Mode>;

        session(asio::io_context& ctxt, 
                msg_cb mcb, err_cb ecb,
                std::vector<uint8_t> key=Botan::hex_decode("2B7E151628AED2A6ABF7158809CF4F3C"))
            : m_socket(ctxt), 
              m_mcb(mcb), m_ecb(ecb) 
        {
            if constexpr (Encrypt == true)
            {
                m_enc = Botan::Cipher_Mode::create("AES-128/CBC/PKCS7", Botan::ENCRYPTION);
                m_dec = Botan::Cipher_Mode::create("AES-128/CBC/PKCS7", Botan::DECRYPTION);

                m_enc->set_key(key);
                m_dec->set_key(key);
            }
        }

        ~session()
        {
            if constexpr (Encrypt == true)
            {
                m_enc.release();
                m_dec.release();
            }
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
                                    std::bind(&session::handle_read_complete, 
                                            enable_shared::shared_from_this(),
                                            std::placeholders::_1,
                                            std::placeholders::_2
                                        )
                                    );                    
        }

        void connect(const asio::ip::tcp::resolver::results_type& endpoints)
        {
            // Request asio attempts to connect to an endpoint
            asio::async_connect(m_socket, endpoints,
                [this](std::error_code ec, asio::ip::tcp::endpoint endpoint)
                {
                    if (!ec)
                    {
                        m_established = true;
                        start();
                    }
                });
        };

        bool is_connected()
        {
            return m_established;
        }

        bool disconnect()
        {
            m_socket.close();
            return true; // TODO
        };
        
        void write(message<T>& msg, wr_cb cb)
        {
            if constexpr (Encrypt)
            {
                if(msg.m_header.m_size)
                {
                    Botan::AutoSeeded_RNG rng;
                    Botan::secure_vector<uint8_t> iv = rng.random_vec(m_enc->default_nonce_length());
                    std::memcpy(&msg.m_header.m_iv[0], iv.data(), iv.size());

                    m_enc->start(iv);
                    m_enc->finish(msg.body());                    
                }
            }
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

        void decrypt(owned_message<T>* owned_msg)
        {
            Botan::secure_vector<uint8_t> iv(&owned_msg->m_msg.m_header.m_iv[0],
                                             &owned_msg->m_msg.m_header.m_iv[16]);
            m_dec->start(iv);
            m_dec->finish(owned_msg->m_msg.body());
        }


        private:
        void handle_read_complete(const asio::error_code& ec,
                         size_t bytes_transferred)
        {
            if (!ec)
            {
                m_mcb(enable_shared::shared_from_this());
                start(); // we've handled the message, start again
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
                if (len)
                {
                    asio::async_write(m_socket,
                        asio::buffer(data, len),
                        std::bind(&session::handle_write_completion,
                            enable_shared::shared_from_this(),
                            cb,
                            std::placeholders::_1
                        )
                    );
                }
                else
                {
                    cb(std::enable_shared_from_this<session<T, Encrypt>>::shared_from_this());
                }
            }
        }

        void handle_write_completion(wr_cb wcb, const asio::error_code& error)
        {
            if (!error)
            {
                wcb(std::enable_shared_from_this<session<T, Encrypt>>::shared_from_this());
            }
        }



    private:
        asio::ip::tcp::socket               m_socket;
        msg_cb                              m_mcb;
        err_cb                              m_ecb;
        asionet::message_header<T>          m_data;
        encrypt_type                        m_enc;
        decrypt_type                        m_dec;

        bool                                m_established{false};
    };
}

#endif
