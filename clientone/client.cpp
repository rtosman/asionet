#include <iostream>
#include <asiomsg.hpp>
#include <asioclient.hpp>
#include <asiostrenc.hpp>
#include <ctime>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <map>
#include "one.hpp"

struct client
{
#if 0 // no encryption asynchronous read
    using sess_type = std::shared_ptr<asionet::session<MsgTypes, false>>;
    using interface_type = asionet::client_interface<MsgTypes, false>;
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

    bool connect(const char* server, uint16_t port, const std::chrono::seconds& tmout)
    {
        std::string s(server);

        return m_intf->connect(s, port, tmout);
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
        std::scoped_lock lock(m_mutex);

        if (m_ping_inflight) return;

        auto& msg = m_outgoing.create_empty_inplace();

        msg.m_header.m_id = MsgTypes::Ping;
        msg << t;

        auto& replies = m_outgoing;
        m_intf->send(msg, [&replies, &msg]()
            {
                replies.slow_erase(msg);
            });

        m_ping_inflight = true;
    }

    void fire_bullet(float x, float y)
    {
        auto& msg = m_outgoing.create_empty_inplace();

        msg.m_header.m_id = MsgTypes::FireBullet;
        msg << x << y;

        auto& replies = m_outgoing;
        m_intf->send(msg, [&replies, &msg]()
            {
                replies.slow_erase(msg);
            });
    }

    void move_player(double x, double y)
    {
        auto& msg = m_outgoing.create_empty_inplace();

        msg.m_header.m_id = MsgTypes::MovePlayer;
        msg << x << y;

        auto& replies = m_outgoing;

        m_intf->send(msg, [&replies, &msg]()
                        {
                            replies.slow_erase(msg);
                        }
                    );
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
        auto& msg = m_outgoing.create_empty_inplace();

        msg.m_header.m_id = MsgTypes::Statistics;

        auto& replies = m_outgoing;

        m_intf->send(msg, [&replies, &msg]()
                        {
                            replies.slow_erase(msg);
                        }
                    );
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
            constexpr auto s1 = asio_make_encrypted_string("Quitting!");
            std::cout << std::string(s1) << "\n";
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
        case ConnectionMade: // let the server know that I know that the server knows that I am connected 
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
                    constexpr auto s1 = asio_make_encrypted_string("Flood ping [ON]");
                    std::cout << std::string(s1) << "\n";
                }
                else
                {
                    constexpr auto s1 = asio_make_encrypted_string("Flood ping [OFF]");
                    std::cout << std::string(s1) << "\n";
                }
            }

            std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
            auto deltams = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<double>(now - m_last_transition));

            if (m_key[Fire] && !m_old_key[Fire])
            {
                m_auto_repeat[Fire] = false;
                m_last_transition = std::chrono::system_clock::now();
                fire_bullet(2.0f, 5.0f);
            }
            else if(!m_auto_repeat[Fire] && 
                    (m_key[Fire] && m_old_key[Fire]) && 
                    (deltams > 500ms))
            {
                m_auto_repeat[Fire] = true;
            }
            else if(m_auto_repeat[Fire] && !m_key[Fire] && !m_old_key[Fire])
            {
                m_last_transition = std::chrono::system_clock::now();                
                m_auto_repeat[Fire] = false;
            }
            else if(m_key[Fire] && m_auto_repeat[Fire])
            {
                fire_bullet(2.0f, 5.0f);
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
                m_last_transition = std::chrono::system_clock::now();                
                m_auto_repeat[Move] = false;
            }
            else if(m_key[Move] && m_auto_repeat[Move])
            {
                move_player(12.0f, 52.0f);
            }

            break;
        }

        for (auto i = 0; i < m_key.size(); ++i) m_old_key[i] = m_key[i];

        return m_run;
    }

private:
    asio::io_context                    m_context;
    std::unique_ptr<interface_type>     m_intf;
    enum {
        ConnectionRequested,
        ConnectionMade,
        ConnectionWaiting,
        ConnectionComplete
    }                                               m_state{ ConnectionRequested };
    std::vector<uint32_t>                           m_ping_times;
    bool                                            m_flood_ping{ false };
    std::array<bool, 6>                             m_key{};
    std::array<bool, 6>                             m_old_key{};
    bool                                            m_run{ true };
    asionet::protqueue<asionet::message<MsgTypes>>  m_outgoing;
    std::chrono::system_clock::time_point           m_last_transition;
    std::array<bool, 6>                             m_auto_repeat{};
    bool                                            m_ping_inflight{ false };
    std::mutex                                      m_mutex{};
    int                                             m_ctf_readiness{0};
    uint8_t                                         m_cdf{ 0 };

    std::map<MsgTypes, apifunc_type> m_apis = {
    { MsgTypes::Invalid, [](sess_type s, asionet::message<MsgTypes>& m)
                            {                    
                                constexpr auto s1 = asio_make_encrypted_string("Invalid message received");

                                std::cerr << std::string(s1) << "\n";
                            }
    },
    { MsgTypes::Connected, [this](sess_type s, asionet::message<MsgTypes>& m)
                            {
                                // The server knows that I know that the server knows that I'm connected
                                constexpr auto s1 = asio_make_encrypted_string("Connected");
                                std::cout << std::string(s1) << "\n";
                                m_state = ConnectionComplete;
                            }
    },
    { MsgTypes::Ping, [this](sess_type s, asionet::message<MsgTypes>& m)
                            {
                                std::scoped_lock lock(m_mutex);
                                std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
                                std::chrono::system_clock::time_point t;
                                m >> t;
                                auto deltaus = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::duration<double>(now - t));
                                m_ping_times.emplace_back(static_cast<uint32_t>(deltaus.count()));
                                if (m_ping_times.size() >= 1000)
                                {
                                    constexpr auto s1 = asio_make_encrypted_string("Ping round trip average = ");
                                    constexpr auto s2 = asio_make_encrypted_string("us (calculated over ");
                                    constexpr auto s3 = asio_make_encrypted_string(" samples)");

                                    std::cout << std::string(s1)
                                        << (std::accumulate(m_ping_times.begin(), m_ping_times.end(), 0) / m_ping_times.size())
                                        << std::string(s2) << m_ping_times.size() << std::string(s3) << "\n";
                                    m_ping_times.clear();
                                }

                                m_ping_inflight = false;
                            }
    },
    { MsgTypes::FireBullet, [this](sess_type s, asionet::message<MsgTypes>& m)
                            {
                                --m_ctf_readiness;
                                std::cout << "response id = " << (uint32_t)m.m_header.m_id << " body is [" << (char*)m.m_body.data() << "]\n";
                                constexpr auto s1 = asio_make_encrypted_string("response id = ");
                                constexpr auto s2 = asio_make_encrypted_string(" body is [");
                                constexpr auto s3 = asio_make_encrypted_string("]");

                                std::cout << std::string(s1) << (uint32_t)m.m_header.m_id << std::string(s2) << (char*)m.m_body.data() << std::string(s3) << "\n";
                            }
    },
    { MsgTypes::MovePlayer, [this](sess_type s, asionet::message<MsgTypes>& m)
                            {
                                ++m_ctf_readiness;
                                std::cout << "response id = " << (uint32_t)m.m_header.m_id << " body is [" << (char*)m.m_body.data() << "]\n";
                            }
    },
    { MsgTypes::CaptureTheFlag, [](sess_type s, asionet::message<MsgTypes>& m)
                            {
                                constexpr auto s1 = asio_make_encrypted_string("response id = ");
                                constexpr auto s2 = asio_make_encrypted_string(" body is [");
                                constexpr auto s3 = asio_make_encrypted_string("]");

                                std::cout << std::string(s1) << (uint32_t)m.m_header.m_id << std::string(s2) << (char*)m.m_body.data() << std::string(s3) << "\n";
                            }
    },
    { MsgTypes::Statistics, [](sess_type s, asionet::message<MsgTypes>& m)
                            {
                                constexpr auto s1 = asio_make_encrypted_string("Peak sessions: ");
                                constexpr auto s2 = asio_make_encrypted_string("Peak messages: ");
                                constexpr auto s3 = asio_make_encrypted_string("Total rx good: ");
                                constexpr auto s4 = asio_make_encrypted_string("Total rx bad : ");
                                asionet::stats stat;
                                
                                m >> stat;

                                std::cout << std::string(s1) << stat.peak_.sessions_ << "\n"
                                          << std::string(s2) << stat.peak_.msgs_ << "\n"
                                          << std::string(s3) << stat.count_.msgs_rx_good_ << "\n"
                                          << std::string(s4) << stat.count_.msgs_rx_bad_ << "\n";
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
    
    try
    {
        c.connect(argv[1], atoi(argv[2]), 10s);

        while (c.run());
    }
    catch (std::exception& e)
    {
        std::cout << "Error: " << e.what() << "\n";
    }

    return 0;
}
