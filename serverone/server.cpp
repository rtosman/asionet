#include <iostream>
#include <asiomsg.hpp>
#include <asioserver.hpp>
#include <asioqueue.hpp>
#include <map>
#include <iomanip>
#include "one.hpp"
#include <chrono>
#include <ctime>

struct server
{
#if 1 // no encryption asynchronous read
    using sess_type = std::shared_ptr<asionet::session<MsgTypes, false>>;
    using interface_type = asionet::server_interface<MsgTypes, false, true>;
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
//        asio::io_service::work work(m_context);
        m_context.run();

        return true;
    }
    asio::io_context                                m_context;
private:

    asionet::protqueue<asionet::message<MsgTypes>>  m_replies;
    std::unique_ptr<interface_type>                 m_intf;
 
    std::map<MsgTypes, apifunc_type> m_apis = {
        { MsgTypes::Invalid, [](sess_type s, asionet::message<MsgTypes>& m) 
                                {
                                    std::cerr << "Invalid message received\n";
                                }
        },
        { MsgTypes::Connected, [this](sess_type s, asionet::message<MsgTypes>& m) 
                                {
                                    std::cerr << "Client is connected\n";
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
                                    std::stringstream ss;

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
                                            if (replies.size() > 1)
                                            {
                                                std::cout << replies.size() << "\n";
                                            }
                                                    replies.slow_erase(reply);
                                                }
                                    );
                                }
        },
        { MsgTypes::FireBullet, [this](sess_type s, asionet::message<MsgTypes>& m) 
                                {
                                    float x{0}, y{0};

                                    m >> y >> x;
                                    std::cout << "Fire Bullet ("
                                        << x << ":" << y
                                        << ") from: " << s->socket().remote_endpoint() << "\n";

                                    auto& reply = m_replies.create_empty_inplace();
                                    reply.m_header.m_id = m.m_header.m_id;
                                    reply << "Fired OK!";
                                    auto& replies = m_replies;
                                    m_intf->send(s, reply,
                                                [&replies, &reply]() -> void
                                                {
                                                std::cout << "FireBullet reply sent\n";
                                                replies.slow_erase(reply);
                                                }
                                    );
                                }
        },
        { MsgTypes::MovePlayer, [this](sess_type s, asionet::message<MsgTypes>& m) 
                                {
                                    double x{0}, y{0};

                                    m >> y >> x;
                                    std::cout << "Move Player ("
                                        << x << ":" << y
                                        << ") from: " << s->socket().remote_endpoint() << "\n";

                                    auto& reply = m_replies.create_empty_inplace();
                                    reply.m_header.m_id = m.m_header.m_id;
                                    reply << "Moved Player OK!";
                                    auto& replies = m_replies;
                                    m_intf->send(s, reply,
                                                [&replies, &reply]() -> void {
                                                std::cout << "MovePlayer reply sent\n";
                                                replies.slow_erase(reply);
                                                }
                                    );
                                }
        },
        { MsgTypes::Statistics, [this](sess_type s, asionet::message<MsgTypes>& m) 
                                {
                                    std::cout << "Statistics\n";

                                    auto& reply = m_replies.create_empty_inplace();
                                    reply.m_header.m_id = m.m_header.m_id;
                                    reply.m_header.m_size = sizeof(asionet::stats);
                                    reply << m_intf->statistics();
                                    auto& replies = m_replies;
                                    m_intf->send(s, reply,
                                                [&replies, &reply]() -> void {
                                                std::cout << "Statistics reply sent\n";
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
        s << ", io_context is " << (game.m_context.stopped() ? "stopped":"running") << "\n";

        std::cout << "Error: " << s.str() << "\n";
    }

    std::cout << argv[0] << " exiting...\n";

    return 0;
}
