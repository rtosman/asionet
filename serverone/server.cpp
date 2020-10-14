#include <iostream>
#include <asiomsg.hpp>
#include <asioserver.hpp>
#include <asioqueue.hpp>
#include <map>
#include <iomanip>

struct server
{
    enum class MsgTypes: uint32_t
    {
        Ping,
        FireBullet,
        MovePlayer
    };
    
    using sess = std::shared_ptr<asionet::session<MsgTypes>>;
    using interface = asionet::server_interface<server::MsgTypes>;
    using apifunc = std::function<void(sess s, asionet::message<MsgTypes>&)>;

    server(uint16_t port):
        m_intf(std::make_unique<interface>(m_ios, port, 
                                [this](sess s)
                                {
                                    std::cout << "New connection\n";
                                    return s->socket().remote_endpoint().address().is_loopback();
                                },
                                [this]() 
                                {
                                    auto m = m_intf->incoming().pop_front();
                                    m_apis[m.m_msg.api()](m.m_remote, m.m_msg);
                                },
                                [this](sess s)
                                {
                                    std::cout << "Connection dropped\n";
                                }
                            )
              )
    {
    }

    bool run()
    {
        m_ios.run();
    }

private:
    asio::io_service                                m_ios; 
    asionet::protqueue<asionet::message<MsgTypes>>  m_replies;
    std::unique_ptr<interface>                      m_intf;
 
    std::map<MsgTypes, apifunc> m_apis = {
        { MsgTypes::Ping, [this](sess s, asionet::message<MsgTypes>& m) 
                                {
                                        // for ping, just send back the message as-is for
                                        // minimal latency
                                        auto& reply = m_replies.create_inplace(m);
                                        auto& replies = m_replies;
                                        s->write(reply, 
                                                 [&replies, &reply](sess s) -> bool {
                                                    replies.slow_erase(reply);
                                                    return true;
                                                 }
                                        );
                                }
        },
        { MsgTypes::FireBullet, [this](sess s, asionet::message<MsgTypes>& m) 
                                {
                                        float x, y;

                                        m >> y >> x;
                                        std::cout << "Fire Bullet ("
                                            << x << ":" << y
                                            << ") from: " << s->socket().remote_endpoint() << "\n";

                                        auto& reply = m_replies.create_empty_inplace();
                                        reply.m_header.m_id = m.m_header.m_id;
                                        reply << "Fired OK!";
                                        auto& replies = m_replies;
                                        s->write(reply, 
                                                 [&replies, &reply](sess s) -> bool {
                                                    std::cout << "FireBullet reply sent\n";
                                                    replies.slow_erase(reply);
                                                    return true;
                                                 }
                                        );
                                }
        },
        { MsgTypes::MovePlayer, [this](sess s, asionet::message<MsgTypes>& m) 
                                {
                                        double x, y;

                                        m >> y >> x;
                                        std::cout << "Move Player ("
                                            << x << ":" << y
                                            << ") from: " << s->socket().remote_endpoint() << "\n";

                                        auto& reply = m_replies.create_empty_inplace();
                                        reply.m_header.m_id = m.m_header.m_id;
                                        reply << "Moved Player OK!";
                                        auto& replies = m_replies;
                                        s->write(reply, 
                                                 [&replies, &reply](sess s) -> bool {
                                                    std::cout << "MovePlayer reply sent\n";
                                                    replies.slow_erase(reply);
                                                    return true;
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
