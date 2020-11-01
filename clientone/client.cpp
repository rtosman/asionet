#include <iostream>
#include <asiomsg.hpp>
#include <asioclient.hpp>
#include <ctime>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <map>
#include "one.hpp"

using namespace std::chrono_literals;

struct client
{
#if 0 // no encryption asynchronous read
    using sess_type = std::shared_ptr<asionet::session<MsgTypes, false>>;
    using interface_type = asionet::client_interface<MsgTypes, false, true>;
    using queue_type = asionet::protqueue<asionet::owned_message<MsgTypes, false>>;
#else // encryption + async read 
    using sess_type = std::shared_ptr<asionet::session<MsgTypes>>;
    using interface_type = asionet::client_interface<MsgTypes>;
    using queue_type = asionet::protqueue<asionet::owned_message<MsgTypes, true>>;
#endif
    using apifunc_type = std::function<void(sess_type s, asionet::message<MsgTypes>&)>;


    const int Ping = 0;
    const int Fire = 1;
    const int Move = 2;
    const int Stat = 3;
    const int Quit = 4;
    const int Capture = 5;

    client():
        m_intf(std::make_unique<interface_type>(m_context,
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

    bool connect(const char* server, uint16_t port)
    {
        std::string s(server);

        return m_intf->connect(s, port);
    }

    bool is_connected()
    {
        return m_intf->is_connected();
    }

    void acknowledge_connection()
    {
        auto& msg = m_outgoing.create_empty_inplace();

        msg.m_header.m_id = MsgTypes::Connected;

        auto& replies = m_outgoing;

        m_intf->send(msg, [&replies, &msg]()
            {
                replies.slow_erase(msg);
            });
    }

    void ping(std::chrono::system_clock::time_point t)
    {
        std::scoped_lock lock(m_state_lock);

        if (m_msg_in_flight[Ping])
        {
            return;
        }

        auto& msg = m_outgoing.create_empty_inplace();

        msg.m_header.m_id = MsgTypes::Ping;
        msg << t;

        auto& replies = m_outgoing;
        m_intf->send(msg, [&replies, &msg]()
            {
                replies.slow_erase(msg);
            });

        m_msg_in_flight[Ping] = true;
    }

    void fire_bullet(float x, float y)
    {
        std::scoped_lock lock(m_state_lock);

        if (m_msg_in_flight[Fire])
        {
            return;
        }

        auto& msg = m_outgoing.create_empty_inplace();

        msg.m_header.m_id = MsgTypes::FireBullet;
        msg << x << y;

        auto& replies = m_outgoing;
        m_intf->send(msg, [&replies, &msg]()
            {
                replies.slow_erase(msg);
            });

        m_msg_in_flight[Fire] = true;
    }

    void move_player(double x, double y)
    {
        std::scoped_lock lock(m_state_lock);

        if (m_msg_in_flight[Move])
        {
            return;
        }

        auto& msg = m_outgoing.create_empty_inplace();

        msg.m_header.m_id = MsgTypes::MovePlayer;
        msg << x << y;

        auto& replies = m_outgoing;

        m_intf->send(msg, [&replies, &msg]()
                        {
                            replies.slow_erase(msg);
                        }
                    );

        m_msg_in_flight[Move] = true;
    }

    void capture_flag(uint8_t a,
                      uint8_t b,
                      uint8_t c,
                      uint8_t d,
                      uint8_t e,
                      uint8_t f,
                      uint8_t g
                     )
    {
        auto& msg = m_outgoing.create_empty_inplace();

        msg.m_header.m_id = MsgTypes::CaptureTheFlag;
        msg << a << b << c << d << e << f << g << m_cdf;

        auto& replies = m_outgoing;

        m_intf->send(msg, [&replies, &msg]() 
                        {
                            replies.slow_erase(msg);
                        }
                    );

    }

    static uint8_t str_to_bin(const char* s)
    {
        uint8_t val = 0;
        for (auto i = 0; i < 8; i++)
        {
            val <<= 1;

            if (s[i] == '1')
            {
                val |= 1;
            }
        }

        return val;
    }

    static uint8_t ascii_one()
    {
        std::string ival("xxzzyyyz");
        std::string oval;

        for (auto v : ival)
        {
            if (v == 0) break;
            if (v == 'x')
            {
                oval += '0';
            }
            else if (v == 'y')
            {
                oval += '0';
            }
            else
            {
                oval += 0x31;
            }
        }

        return str_to_bin(oval.c_str());
    }


    static uint8_t ascii_capital_a()
    {
        std::string ival("xzyyyyyz");
        std::string oval;

        for (auto v : ival)
        {
            if (v == 0) break;
            if (v == 'x')
            {
                oval += '0';
            }
            else if (v == 'y')
            {
                oval += '0';
            }
            else
            {
                oval += ascii_one();
            }
        }

        return str_to_bin(oval.c_str());
    }

    static uint8_t ascii_capital_b()
    {
        std::string ival("xzyyyyzx");
        std::string oval;

        for (auto v : ival)
        {
            if (v == 0) break;
            if (v == 'x')
            {
                oval += '0';
            }
            else if (v == 'y')
            {
                oval += '0';
            }
            else
            {
                oval += ascii_one();
            }
        }

        return str_to_bin(oval.c_str());
    }

    static uint8_t ascii_capital_c()
    {
        std::string ival("xzyyyyzz");
        std::string oval;

        for (auto v : ival)
        {
            if (v == 0) break;
            if (v == 'x')
            {
                oval += '0';
            }
            else if (v == 'y')
            {
                oval += '0';
            }
            else
            {
                oval += ascii_one();
            }
        }

        return str_to_bin(oval.c_str());
    }

    static uint8_t ascii_capital_d()
    {
        std::string ival("xzyyyzxx");
        std::string oval;

        for (auto v : ival)
        {
            if (v == 0) break;
            if (v == 'x')
            {
                oval += '0';
            }
            else if (v == 'y')
            {
                oval += '0';
            }
            else
            {
                oval += ascii_one();
            }
        }

        return str_to_bin(oval.c_str());
    }

    static uint8_t ascii_capital_e()
    {
        std::string ival("xzyyyzxz");
        std::string oval;

        for (auto v : ival)
        {
            if (v == 0) break;
            if (v == 'x')
            {
                oval += '0';
            }
            else if (v == 'y')
            {
                oval += '0';
            }
            else
            {
                oval += ascii_one();
            }
        }

        return str_to_bin(oval.c_str());
    }

    static uint8_t ascii_capital_f()
    {
        std::string ival("xzyyyzzx");
        std::string oval;

        for (auto v : ival)
        {
            if (v == 0) break;
            if (v == 'x')
            {
                oval += '0';
            }
            else if (v == 'y')
            {
                oval += '0';
            }
            else
            {
                oval += ascii_one();
            }
        }

        return str_to_bin(oval.c_str());
    }

    static uint8_t ascii_capital_g()
    {
        std::string ival("xzyyyzzz");
        std::string oval;

        for (auto v : ival)
        {
            if (v == 0) break;
            if (v == 'x')
            {
                oval += '0';
            }
            else if (v == 'y')
            {
                oval += '0';
            }
            else
            {
                oval += ascii_one();
            }
        }

        return str_to_bin(oval.c_str());
    }

    static uint8_t ascii_capital_k()
    {
        std::string ival("xzyyzxzz");
        std::string oval;

        for (auto v : ival)
        {
            if (v == 0) break;
            if (v == 'x')
            {
                oval += '0';
            }
            else if (v == 'y')
            {
                oval += '0';
            }
            else
            {
                oval += ascii_one();
            }
        }

        return str_to_bin(oval.c_str());
    }

    static uint32_t get_3()
    {
        std::string ival("KGGLdKzz");
        std::string oval;

        for (auto v : ival)
        {
            if (v == 0) break;
            if (v == 'h')
            {
                oval += '0';
            }
            else if (v == ascii_capital_g())
            {
                oval += '0';
            }
            else if (v == 'L')
            {
                oval += '0';
            }
            else if (v == ascii_capital_k())
            {
                oval += '0';
            }
            else if (v == 'd')
            {
                oval += '0';
            }
            else
            {
                oval += ascii_one();
            }
        }

        return str_to_bin(oval.c_str());
    }

    // static predamage
    void get_ctf_sequence()
    {
        // repair ascii_capital_* here
        uint8_t a,b,c,d,e,f,g;

        std::cout << ascii_capital_a() << ":\n";
        std::cin >> a;
        std::cout << ascii_capital_b() << ":\n";
        std::cin >> b;
        std::cout << ascii_capital_c() << ":\n";
        std::cin >> c;
        std::cout << ascii_capital_d() << ":\n";
        std::cin >> d;
        std::cout << ascii_capital_e() << ":\n";
        std::cin >> e;
        std::cout << ascii_capital_f() << ":\n";
        std::cin >> f;
        std::cout << ascii_capital_g() << ":\n";
        std::cin >> g;
        
        // redamage ascii_capital_* here and repair
        // capture flag here
        capture_flag(a, b, c, d, e, f, g);
        // redamage capture_flag here
    }

    void get_statistics()
    {
        std::scoped_lock lock(m_state_lock);

        if (m_msg_in_flight[Stat])
        {
            return;
        }

        auto& msg = m_outgoing.create_empty_inplace();

        msg.m_header.m_id = MsgTypes::Statistics;

        auto& replies = m_outgoing;

        m_intf->send(msg, [&replies, &msg]()
                        {
                            replies.slow_erase(msg);
                        }
                    );

        m_msg_in_flight[Stat] = true;
    }

    bool run()
    {
        if (GetForegroundWindow() == GetConsoleWindow())
        {
            m_key[Ping] = GetAsyncKeyState('P') & 0x8000;
            m_key[Fire] = GetAsyncKeyState('F') & 0x8000;
            m_key[Move] = GetAsyncKeyState('M') & 0x8000;
            m_key[Stat] = GetAsyncKeyState('S') & 0x8000;
            m_key[Quit] = GetAsyncKeyState('Q') & 0x8000;
            m_key[Capture] = GetAsyncKeyState(ascii_capital_k()) & 0x8000;
        }

        if ((m_key[Quit] && !m_old_key[Quit]))
        {
            std::cout << "Quitting!\n";
            m_run = false;
        }

        if (m_flood_ping)
        {
            ping(std::chrono::system_clock::now());
        }

        switch (m_state)
        {
        case ConnectionRequested:
            if (is_connected()) m_state = ConnectionMade;
            break;
        case ConnectionMade:
            acknowledge_connection();
            m_state = ConnectionWaiting;
            break;
        case ConnectionWaiting:
            break;
        case ConnectionComplete:
            if (m_key[Stat] && !m_old_key[Stat])
            {
                get_statistics();
            }

            if (m_key[Ping] && !m_old_key[Ping])
            {
                m_flood_ping = !m_flood_ping;
                if (m_flood_ping)
                {
                    std::cout << "Flood ping [ON]\n";
                }
                else
                {
                    std::cout << "Flood ping [OFF]\n";
                }
            }

            if (m_key[Fire] && !m_old_key[Fire])
            {
                std::string x,y;

                std::cout << "Enter X:\n";
                std::cin >> x;
                std::cout << "Enter Y:\n";
                std::cin >> y;

                fire_bullet(std::stof(x), std::stof(y));
            }
            if (m_key[Move] && !m_old_key[Move])
            {
                std::string x,y;

                std::cout << "Enter X:\n";
                std::cin >> x;
                std::cout << "Enter Y:\n";
                std::cin >> y;
                
                move_player(std::stod(x), std::stod(y));
            }            
            if (m_key[Capture] && !m_old_key[Capture])
            {
                if(m_ctf_readiness == get_3())
                {
                    // put label here to invoke repair of get_ctf_sequence
                    get_ctf_sequence();
                    // put label here to invoke redamage of get_ctf_sequence
                }
                --m_ctf_readiness;
            }

            break;
        }

        for (auto i = 0; i < sizeof(m_key) / sizeof(m_key[Ping]); ++i) m_old_key[i] = m_key[i];

        return m_run;
    }

private:
    asio::io_context                         m_context;
    std::unique_ptr<interface_type>          m_intf;
    enum {
        ConnectionRequested,
        ConnectionMade,
        ConnectionWaiting,
        ConnectionComplete
    }                                               m_state{ ConnectionRequested };
    std::vector<uint32_t>                           m_ping_times;
    bool                                            m_flood_ping{ false };
    std::array<bool, 6>                             m_msg_in_flight{};
    std::array<bool, 6>                             m_key{};
    std::array<bool, 6>                             m_old_key{};
    bool                                            m_run{ true };
    asionet::protqueue<asionet::message<MsgTypes>>  m_outgoing;
    std::mutex                                      m_state_lock;
    int                                             m_ctf_readiness{0};
    uint8_t                                         m_cdf{ 0 };

    std::map<MsgTypes, apifunc_type> m_apis = {
    { MsgTypes::Invalid, [](sess_type s, asionet::message<MsgTypes>& m)
                            {
                                std::cerr << "Invalid message received\n";
                            }
    },
    { MsgTypes::Connected, [this](sess_type s, asionet::message<MsgTypes>& m)
                            {
                                std::cout << "Connected \n";
                                m_state = ConnectionComplete;
                            }
    },
    { MsgTypes::Ping, [this](sess_type s, asionet::message<MsgTypes>& m)
                            {
                                std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
                                std::chrono::system_clock::time_point t;
                                m >> t;
                                auto deltaus = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::duration<double>(now - t));
                                std::scoped_lock lock(m_state_lock);
                                m_ping_times.emplace_back(static_cast<uint32_t>(deltaus.count()));
                                if (m_ping_times.size() >= 1000)
                                {
                                    std::cout << "Ping round trip average = "
                                              << (std::accumulate(m_ping_times.begin(), m_ping_times.end(), 0) / m_ping_times.size())
                                              << "us (calculated over " << m_ping_times.size() << " samples)\n";
                                    m_ping_times.clear();
                                }
                                m_msg_in_flight[Ping] = false;
                            }
    },
    { MsgTypes::FireBullet, [this](sess_type s, asionet::message<MsgTypes>& m)
                            {
                                --m_ctf_readiness;
                                std::cout << "response id = " << (uint32_t)m.m_header.m_id << " body is [" << (char*)m.m_body.data() << "]\n";
                                m_msg_in_flight[Fire] = false;
                            }
    },
    { MsgTypes::MovePlayer, [this](sess_type s, asionet::message<MsgTypes>& m)
                            {
                                ++m_ctf_readiness;
                                std::cout << "response id = " << (uint32_t)m.m_header.m_id << " body is [" << (char*)m.m_body.data() << "]\n";
                                m_msg_in_flight[Move] = false;
                            }
    },
    { MsgTypes::CaptureTheFlag, [this](sess_type s, asionet::message<MsgTypes>& m)
                            {
                                std::cout << "response id = " << (uint32_t)m.m_header.m_id << " body is [" << (char*)m.m_body.data() << "]\n";
                                m_msg_in_flight[Capture] = false;
                            }
    },
    { MsgTypes::Statistics, [this](sess_type s, asionet::message<MsgTypes>& m)
                            {
                                asionet::stats stat;
                                
                                m >> stat;

                                std::cout << "Peak sessions: " << stat.peak_.sessions_ << "\n"
                                          << "Peak messages: " << stat.peak_.msgs_ << "\n"
                                          << "Total rx good: " << stat.count_.msgs_rx_good_ << "\n"
                                          << "Total rx bad : " << stat.count_.msgs_rx_bad_ << "\n";

                                m_msg_in_flight[Stat] = false;
                            }
    }
    };

    const char* decrypt_flag(const char* encrypted_content)
    {
        // this is where transformit needs to go to decrypt the flag
        return encrypted_content;
    }

 };

int main(int argc, char** argv)
{
    client c;
    
    c.connect(argv[1], atoi(argv[2]));

    while (c.run());

    return 0;
}
