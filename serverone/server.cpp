#include <iostream>
#include <asiomsg.hpp>
#include <asioserver.hpp>
#include <asioqueue.hpp>
#include <asiostrenc.hpp>
#include <map>
#include <iomanip>
#include "one.hpp"
#include <chrono>
#include <ctime>

struct server
{
#if 0 // no encryption asynchronous read
    using sess_type = std::shared_ptr<asionet::session<MsgTypes, false>>;
    using interface_type = asionet::server_interface<MsgTypes, false>;
    using queue_type = asionet::protqueue<asionet::owned_message<MsgTypes, false>>;
#else // encryption + async read 
    using sess_type = std::shared_ptr<asionet::session<MsgTypes>>;
    using interface_type = asionet::server_interface<MsgTypes>;
    using queue_type = asionet::protqueue<asionet::owned_message<MsgTypes, true>>;
#endif
    using apifunc_type = std::function<void(sess_type s, asionet::message<MsgTypes>&)>;

    server(uint16_t port):
        m_intf(std::make_unique<interface_type>(m_context, port, 
                                [](sess_type s)
                                {
                                    std::cout << "New connection request\n";
                                    return s->socket().remote_endpoint().address().is_loopback();
                                },
                                [this](queue_type& queue)
                                {
                                    auto& m = queue.front();
                                    m_apis[clamp_msg_types(m.m_msg.api())](m.m_remote, m.m_msg);
                                    queue.pop_front();
                                },
                                [](sess_type s)
                                {
                                    std::cout << "Connection dropped\n";
                                }
                            )
              )
    {
    }

    bool run()
    {
        m_context.run();

        return true;
    }

    bool stopped()
    {
        return m_context.stopped();
    }

private:
    asio::io_context                                m_context;
    asionet::protqueue<asionet::message<MsgTypes>>  m_replies;
    std::unique_ptr<interface_type>                 m_intf;
 
    std::map<MsgTypes, apifunc_type> m_apis = {
        { MsgTypes::Invalid, [](sess_type s, asionet::message<MsgTypes>& m) 
                                {
                                    constexpr auto s1 = asio_make_encrypted_string("Invalid message received");
                                    std::cerr << std::string(s1) << "\n";
                                }
        },
        { MsgTypes::Connected, [this](sess_type s, asionet::message<MsgTypes>& m) 
                                {
                                    constexpr auto s1 = asio_make_encrypted_string("Client is connected");
                                    std::cerr << std::string(s1) << "\n";
                                    auto& reply = m_replies.create_inplace(m.m_header);
                                    auto& replies = m_replies;
                                    reply.blank(); // Connected reply has no data
                                    m_intf->send(s, reply, 
                                            [&replies, &reply]() -> void
                                            {
                                                replies.slow_erase(reply);
                                            }
                                        );
                                }
        },
        { MsgTypes::Ping, [this](sess_type s, asionet::message<MsgTypes>& m) 
                                {
                                    std::chrono::system_clock::time_point t;

                                    m >> t;
                                    // for ping, just send back the message as-is for
                                    // minimal latency
                                    auto& reply = m_replies.create_inplace(m.m_header);
                                    reply << t;
                                    auto& replies = m_replies;
                                    m_intf->send(s, reply,
                                                [&replies, &reply]() -> void
                                                {
                                                    replies.slow_erase(reply);
                                                }
                                    );
                                }
        },
        { MsgTypes::FireBullet, [this](sess_type s, asionet::message<MsgTypes>& m) 
                                {
                                    constexpr auto s1 = asio_make_encrypted_string("Fire Bullet (");
                                    constexpr auto s2 = asio_make_encrypted_string(") from: ");
                                    constexpr auto s3 = asio_make_encrypted_string("Fired OK!");
                                    float x{0}, y{0};

                                    m >> y >> x;
                                    std::cout << std::string(s1)
                                        << x << ":" << y
                                        << std::string(s2) << s->socket().remote_endpoint() << "\n";

                                    auto& reply = m_replies.create_inplace(m.m_header);
                                    reply << std::string(s3).c_str();
                                    auto& replies = m_replies;
                                    m_intf->send(s, reply,
                                                [&replies, &reply]() -> void
                                                {
                                                    constexpr auto s4 = asio_make_encrypted_string("FireBullet reply sent");
                                                    std::cout << std::string(s4) << "\n";
                                                    replies.slow_erase(reply);
                                                }
                                    );
                                }
        },
        { MsgTypes::MovePlayer, [this](sess_type s, asionet::message<MsgTypes>& m) 
                                {
                                    constexpr auto s1 = asio_make_encrypted_string("Move Player (");
                                    constexpr auto s2 = asio_make_encrypted_string(") from: ");
                                    constexpr auto s3 = asio_make_encrypted_string("Moved Player OK!");
                                    double x{0}, y{0};

                                    m >> y >> x;
                                    std::cout << std::string(s1)
                                        << x << ":" << y
                                        << std::string(s2) << s->socket().remote_endpoint() << "\n";

                                    auto& reply = m_replies.create_inplace(m.m_header);

                                    reply << std::string(s3).c_str();
                                    auto& replies = m_replies;
                                    m_intf->send(s, reply,
                                                [&replies, &reply]() -> void {
                                                    constexpr auto s4 = asio_make_encrypted_string("MovePlayer reply sent");
                                                    std::cout << std::string(s4) << "\n";
                                                    replies.slow_erase(reply);
                                                }
                                    );
                                }
        },
        { MsgTypes::Statistics, [this](sess_type s, asionet::message<MsgTypes>& m) 
                                {
                                    constexpr auto s1 = asio_make_encrypted_string("Request Statistics from ");

                                    std::cout << std::string(s1)
                                              << s->socket().remote_endpoint() << "\n";

                                    auto& reply = m_replies.create_inplace(m.m_header);
                                    reply << m_intf->statistics();
                                    auto& replies = m_replies;
                                    m_intf->send(s, reply,
                                                [&replies, &reply]() -> void {
                                                    constexpr auto s2 = asio_make_encrypted_string("Statistics reply sent");
                                                    std::cout << std::string(s2) << "\n";
                                                    replies.slow_erase(reply);
                                                }
                                    );
                                }
        }
    };
};

int main(int argc, char** argv)
{
    server game(atoi(argv[1]));

    try
    {
        game.run();
    }
    catch (std::exception& e)
    {
        std::stringstream s;
        s << e.what();
        s << ", io_context is " << (game.stopped() ? "stopped":"running") << "\n";

        std::cout << "Error: " << s.str() << "\n";
    }

    std::cout << argv[0] << " exiting...\n";

    return 0;
}
