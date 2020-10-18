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

#if 0 // no encryption synchronous read
    using sess = std::shared_ptr<asionet::session<MsgTypes, false>>;
    using interface = asionet::server_interface<MsgTypes, false, false>;
#else // encryption + async read 
    using sess = std::shared_ptr<asionet::session<MsgTypes>>;
    using interface = asionet::server_interface<MsgTypes>;
#endif
    using apifunc = std::function<void(sess s, asionet::message<MsgTypes>&)>;

    server(uint16_t port):
        m_intf(std::make_unique<interface>(m_context, port, 
                                [](sess s)
                                {
                                    std::cout << "New connection request\n";
                                    return s->socket().remote_endpoint().address().is_loopback();
                                },
                                [this]() 
                                {
                                    auto m = m_intf->incoming().pop_front();
                                    m_apis[clamp_msg_types(m.m_msg.api())](m.m_remote, m.m_msg);
                                },
                                [](sess s)
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

private:
    asio::io_context                                m_context; 
    asionet::protqueue<asionet::message<MsgTypes>>  m_replies;
    std::unique_ptr<interface>                      m_intf;
 
    std::map<MsgTypes, apifunc> m_apis = {
        { MsgTypes::Invalid, [](sess s, asionet::message<MsgTypes>& m) 
                                {
                                    std::cerr << "Invalid message received\n";
                                }
        },
        { MsgTypes::Connected, [this](sess s, asionet::message<MsgTypes>& m) 
                                {
                                    std::cerr << "Client is connected\n";
                                    auto& reply = m_replies.create_inplace(m);
                                    auto& replies = m_replies;
                                    reply.blank(); // Connected reply has no data
                                    s->write(reply, 
                                            [&replies, &reply](sess s) -> void 
                                            {
                                                replies.slow_erase(reply);
                                            }
                                        );
                                }
        },
        { MsgTypes::Ping, [this](sess s, asionet::message<MsgTypes>& m) 
                                {
                                        // for ping, just send back the message as-is for
                                        // minimal latency, the ping is only a header 
                                        // so encryption will not be applied
                                        auto& reply = m_replies.create_inplace(m);
                                        auto& replies = m_replies;
                                        s->write(reply, 
                                                 [&replies, &reply](sess s) -> void 
                                                 {
                                                    replies.slow_erase(reply);
                                                 }
                                        );
                                }
        },
        { MsgTypes::FireBullet, [this](sess s, asionet::message<MsgTypes>& m) 
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
                                        s->write(reply, 
                                                 [&replies, &reply](sess s) -> void 
                                                 {
                                                    std::cout << "FireBullet reply sent\n";
                                                    replies.slow_erase(reply);
                                                 }
                                        );
                                }
        },
        { MsgTypes::MovePlayer, [this](sess s, asionet::message<MsgTypes>& m) 
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
                                        s->write(reply, 
                                                 [&replies, &reply](sess s) -> void {
                                                    std::cout << "MovePlayer reply sent\n";
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

    game.run();

    std::cout << argv[0] << " exiting...\n";

    return 0;
}
