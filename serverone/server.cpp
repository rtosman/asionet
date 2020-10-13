#include <iostream>
#include <asiomsg.hpp>
#include <asioserver.hpp>
#include <asioqueue.hpp>
#include <map>

struct server
{
    enum class MsgTypes: uint32_t
    {
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
                                    return true;
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
        m_ios.run();
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
        { MsgTypes::FireBullet, [this](sess s, asionet::message<MsgTypes>& m) 
                                {
                                        float x, y;

                                        m >> y >> x;
                                        std::cout << "Fire Bullet ("
                                            << x << ":" << y
                                            << ")\n";

                                        auto [reply, iter] = m_replies.create_empty_inplace();
                                        reply.m_header.m_id = m.m_header.m_id;
                                        reply << "Fired OK!";
                                        auto& replies = m_replies;
                                        auto i = iter;
                                        s->write(reply, 
                                                 [&replies, i](sess s) -> bool {
                                                     std::cout << "FireBullet reply sent\n";
                                                     replies.erase(i);
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
                                            << ")\n";

                                        auto [reply, iter] = m_replies.create_empty_inplace();
                                        reply.m_header.m_id = m.m_header.m_id;
                                        reply << "Moved Player OK!";
                                        auto& replies = m_replies;
                                        auto i = iter;
                                        s->write(reply, 
                                                 [&replies, i](sess s) -> bool {
                                                     std::cout << "MovePlayer reply sent\n";
                                                     replies.erase(i);
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
