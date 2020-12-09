#ifndef _ASIOSESSION_HPP_INCLUDED
#define _ASIOSESSION_HPP_INCLUDED
#include "asionet.hpp"
#include "asioqueue.hpp"
#include "asiomsg.hpp"
#include "asionetcrypto.hpp"
#include <cstdint>
#include <condition_variable>

namespace asionet 
{

    const Botan::secure_vector<uint8_t> KnownIv = {0,4,7,2,0xa,0xba,0xab,0x42,1,3,6,5,0xf,0xef,0xdb,0x23};
                

    template <typename T, bool Encrypt>
    struct session : public std::enable_shared_from_this<session<T, Encrypt>>
    {
        using msg_cb = std::function<void(std::shared_ptr<session<T, Encrypt>>)>;
        using wr_cb = std::function<void(std::shared_ptr<session<T, Encrypt>>, const asio::error_code&)>;
        using err_cb = std::function<void(std::shared_ptr<session<T, Encrypt>>)>;
        using enable_shared = std::enable_shared_from_this<session<T, Encrypt>>;
        using encrypt_type = std::unique_ptr<Botan::Cipher_Mode>;
        using decrypt_type = std::unique_ptr<Botan::Cipher_Mode>;

        session(asio::io_context& ctxt, asio::io_context::strand& read, asio::io_context::strand& write,
            msg_cb mcb, err_cb ecb,
            std::vector<uint8_t> key = Botan::hex_decode("2B7E151628AED2A6ABF7158809CF4F3C"))
            : m_read(read), m_write(write), m_socket(ctxt),
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
            m_socket.close();
        }

        asio::ip::tcp::socket& socket()
        {
            return m_socket;
        }

        const message_header<T>& get_hdr() const
        {
            return m_data;
        }

        void start()
        {
            m_established = true;
            asio::async_read(m_socket,
                asio::buffer(&m_data, sizeof m_data),
                asio::bind_executor(m_read, 
                                    std::bind(&session::handle_read_complete,
                                                enable_shared::shared_from_this(),
                                                std::placeholders::_1,
                                                std::placeholders::_2
                                             )
                                 )
            );
        }

        template <typename F1>
        void connect(const asio::ip::tcp::resolver::results_type& endpoints, F1 cb)
        {
            // Request asio attempts to connect to an endpoint
            asio::async_connect(m_socket, endpoints,
                                [cb](std::error_code ec, asio::ip::tcp::endpoint endpoint)
                                {
                                    if (!ec)
                                    {
                                        cb();
                                    }
                                }
                               );
        };

        bool is_connected() const
        {
            return m_established;
        }

        bool disconnect()
        {
            m_socket.close();
            return true; // TODO
        };

        template <typename F1>
        void send(std::shared_ptr<message<T>> msg, F1 cb)
        {
            // the condvar and lock ensure that header + body are atomic on the wire
            std::unique_lock<std::mutex> lock(m_send_mutex);
            auto& send_pending = m_send_pending;
            if (m_cv.wait_for(lock, 100ms, [&send_pending] { return !send_pending; }))
            {
                [[likely]]
                m_send_pending = true;
                lock.unlock();
                auto& send_mutex = m_send_mutex;
                auto& cv = m_cv;
                write(msg, asio::bind_executor(m_write, [&send_pending, &send_mutex, &cv, cb](std::shared_ptr<session<T, Encrypt>> s, const asio::error_code& ec)
                                                {
                                                    send_mutex.lock();
                                                    send_pending = false;
                                                    send_mutex.unlock();
                                                    cv.notify_all();
                                                    cb();
                                                }
                                              )
                     );
            }
            else
            {
                [[unlikely]]
                throw std::exception("(I/O) wait timed out on send");
            }
        }

        void decrypt(message<T>& msg)
        {
            try
            {
                Botan::secure_vector<uint8_t> iv(&msg.m_header.m_iv[0],
                    &msg.m_header.m_iv[16]);
                m_dec->start(iv);
                m_dec->finish(msg.body());
            }
            catch (Botan::Decoding_Error& e)
            {
                std::cout << "Decoding error: " << e.what()
                    << " msg ["
                    << Botan::hex_encode(msg.body())
                    << "] unencrypted length: " << msg.m_header.m_size
                    << " encrypted length: " << msg.body().size()
                    << "\n";
            }
        }

        Botan::secure_vector<uint8_t> decrypt(uint8_t* data, size_t size)
        {
            Botan::secure_vector<uint8_t> candidate(data, &data[size]);
            try
            {
                m_dec->start(KnownIv);
                m_dec->finish(candidate);
            }
            catch (Botan::Decoding_Error& e)
            {
                // this is just an authentication failure (expected)
            }
            return candidate;
        }

        Botan::secure_vector<uint8_t> encrypt(uint8_t* data, size_t size)
        {
            Botan::secure_vector<uint8_t> candidate(data, &data[size]);
            m_enc->start(KnownIv);
            m_enc->finish(candidate);
            return candidate;
        }

        void current_challenge(std::tuple<std::shared_ptr<uint8_t>, size_t> data)
        {
            m_cur_challenge = data;
        }

        const std::tuple<std::shared_ptr<uint8_t>, size_t>& current_challenge() const
        {
            return m_cur_challenge;
        }

        asio::io_context::strand& rd_strand()
        {
            return m_read;
        }

        asio::io_context::strand& wr_strand()
        {
            return m_write;
        }

    private:

        void write(std::shared_ptr<message<T>> msg, wr_cb cb)
        {
            if constexpr (Encrypt)
            {
                if (msg->m_header.m_size)
                {
                    Botan::AutoSeeded_RNG rng;
                    Botan::secure_vector<uint8_t> iv = rng.random_vec(m_enc->default_nonce_length());
                    std::memcpy(&msg->m_header.m_iv[0], iv.data(), iv.size());

                    m_enc->start(iv);
                    m_enc->finish(msg->body());
                }
            }
            asio::async_write(m_socket,
                              asio::buffer((uint8_t*)msg.get(), sizeof msg->m_header),
                              asio::bind_executor(m_write,
                                                  std::bind(&session::handle_body,
                                                              enable_shared::shared_from_this(),
                                                              msg->m_body.data(),
                                                              msg->m_body.size(),
                                                              cb,
                                                              std::placeholders::_1
                                                            )
                                                 )
            );
        }

        void handle_read_complete(const asio::error_code& ec,
                                  size_t bytes_transferred)
        {
            if (!ec)
            {
                [[likely]]
                m_mcb(enable_shared::shared_from_this());
            }
            else
            {
                [[unlikely]]
                m_ecb(enable_shared::shared_from_this());
            }
        }

        void handle_body(uint8_t* data, size_t len, wr_cb cb, 
                         const asio::error_code& ec)
        {  
            if (len) // write the body
            {
                [[likely]]
                asio::async_write(m_socket,
                                  asio::buffer(data, len),
                                  asio::bind_executor(m_write,
                                                      std::bind(&session::handle_write_completion,
                                                                enable_shared::shared_from_this(),
                                                                cb,
                                                                std::placeholders::_1
                                                               )
                                                     )
                );
            }
            else // header only message, we're done
            {
                [[unlikely]]
                cb(enable_shared::shared_from_this(), ec);
            }
        }

        void handle_write_completion(wr_cb wcb, const asio::error_code& ec)
        {
            wcb(enable_shared::shared_from_this(), ec);
        }
        
        asio::io_context::strand&                       m_read;
        asio::io_context::strand&                       m_write;
        asio::ip::tcp::socket                           m_socket;
        msg_cb                                          m_mcb;
        err_cb                                          m_ecb;
        asionet::message_header<T>                      m_data;
        encrypt_type                                    m_enc;
        decrypt_type                                    m_dec;
        bool                                            m_established{false};
        std::condition_variable                         m_cv;
        std::mutex                                      m_send_mutex;
        bool                                            m_send_pending{ false };
        std::tuple<std::shared_ptr<uint8_t>, size_t>    m_cur_challenge;
    };
}

#endif
