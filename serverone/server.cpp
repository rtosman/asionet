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
                                    return true;
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

    void run()
    {
        m_intf->run();
    }

    bool stopped()
    {
        return m_context.stopped();
    }

    ~server()
    {
    }
private:
    asio::io_context                                        m_context;
    std::vector<std::unique_ptr<asio::io_context::strand>>  m_rd_strands;
    std::vector<std::unique_ptr<asio::io_context::strand>>  m_wr_strands;
    std::unique_ptr<interface_type>                         m_intf;
 
    std::map<MsgTypes, apifunc_type> m_apis = {
        { MsgTypes::Invalid, [](sess_type s, asionet::message<MsgTypes>& m) 
                                {
                                    constexpr auto s1 = asionet_make_encrypted_string("Invalid message received");
                                    std::cerr << std::string(s1) << "\n";
                                }
        },
        { MsgTypes::Connected, [this](sess_type s, asionet::message<MsgTypes>& m) 
                                {
                                    constexpr auto s1 = asionet_make_encrypted_string("Client is connected");
                                    std::cerr << std::string(s1) << "\n";

                                    auto r = std::make_shared<asionet::message<MsgTypes>>(m.m_header);
                                    r->blank();
                                    m_intf->send(s, r, [r]() -> void {});
                                }
        },
        { MsgTypes::Ping, [this](sess_type s, asionet::message<MsgTypes>& m) 
                                {
                                    std::chrono::system_clock::time_point t;

                                    m >> t;

                                    auto r = std::make_shared<asionet::message<MsgTypes>>(m.m_header);
                                    *r << t;
                                    m_intf->send(s, r, [r]() -> void {});
                                }
        },
        { MsgTypes::FireBullet, [this](sess_type s, asionet::message<MsgTypes>& m) 
                                {
                                    constexpr auto s1 = asionet_make_encrypted_string("Fire Bullet (");
                                    constexpr auto s2 = asionet_make_encrypted_string(") from: ");
                                    constexpr auto fired = asionet_make_encrypted_string("Fired!");
                                    float x{0}, y{0};

                                    m >> y >> x;
                                    std::cout << std::string(s1)
                                        << x << ":" << y
                                        << std::string(s2) << s->socket().remote_endpoint() << "\n";

                                    auto r = std::make_shared<asionet::message<MsgTypes>>(m.m_header);
                                    *r << std::string(fired).c_str();
                                    m_intf->send(s, r,
                                                [r]() -> void
                                                {
                                                    constexpr auto s3 = asionet_make_encrypted_string("FireBullet reply sent");
                                                    std::cout << std::string(s3) << "\n";
                                                }
                                    );
                                }
        },
        { MsgTypes::MovePlayer, [this](sess_type s, asionet::message<MsgTypes>& m) 
                                {
                                    constexpr auto s1 = asionet_make_encrypted_string("Move Player (");
                                    constexpr auto s2 = asionet_make_encrypted_string(") from: ");
                                    constexpr auto moved = asionet_make_encrypted_string("Moved Player!");
                                    double x{0}, y{0};

                                    m >> y >> x;
                                    std::cout << std::string(s1)
                                        << x << ":" << y
                                        << std::string(s2) << s->socket().remote_endpoint() << "\n";

                                    auto r = std::make_shared<asionet::message<MsgTypes>>(m.m_header);
                                    *r << std::string(moved).c_str();
                                    m_intf->send(s, r,
                                                [r]() -> void {
                                                    constexpr auto s3 = asionet_make_encrypted_string("MovePlayer reply sent");
                                                    std::cout << std::string(s3) << "\n";
                                                }
                                    );
                                }
        },
        { MsgTypes::Statistics, [this](sess_type s, asionet::message<MsgTypes>& m) 
                                {
                                    constexpr auto s1 = asionet_make_encrypted_string("Request Statistics from ");

                                    std::cout << std::string(s1)
                                              << s->socket().remote_endpoint() << "\n";

                                    // pretend something takes a long time
                                    // for(int i=0; i < 100; ++i)
                                    // {
                                    //     std::this_thread::sleep_for(10ms);
                                    // }

                                    auto r = std::make_shared<asionet::message<MsgTypes>>(m.m_header);
                                    *r << m_intf->statistics();
                                    m_intf->send(s, r,
                                                [r]() -> void {
                                                    constexpr auto s2 = asionet_make_encrypted_string("Statistics reply sent");
                                                    std::cout << std::string(s2) << "\n";
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
    catch(...)
    {
        std::cout << "Unhandled exception\n";
    }

    std::cout << argv[0] << " exiting...\n";

    return 0;
}
