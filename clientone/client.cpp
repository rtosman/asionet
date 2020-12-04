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
                                                constexpr auto s1 = asionet_make_encrypted_string("Connection dropped!");
                                                std::cout << std::string(s1) << "\n";
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

    bool in_flood_ping()
    {
        return m_flood_ping;
    }

    void acknowledge_connection()
    {
        auto m = std::make_shared<asionet::message<MsgTypes>>();

        m->m_header.m_id = MsgTypes::Connected;

        m_intf->send(m, [m]() -> void {});
    }

    void ping(std::chrono::system_clock::time_point t)
    {
        std::scoped_lock lock(m_mutex);

        if (m_ping_inflight) return;

        auto m = std::make_shared<asionet::message<MsgTypes>>();

        m->m_header.m_id = MsgTypes::Ping;
        *m << t;

        m_intf->send(m, [m]() -> void {});

        m_ping_inflight = true;
    }

    void fire_bullet(float x, float y)
    {
        auto m = std::make_shared<asionet::message<MsgTypes>>();

        m->m_header.m_id = MsgTypes::FireBullet;
        *m << x << y;

        m_intf->send(m, [m]() -> void {});

    }

    void move_player(double x, double y)
    {
        auto m = std::make_shared<asionet::message<MsgTypes>>();

        m->m_header.m_id = MsgTypes::MovePlayer;
        *m << x << y;

        m_intf->send(m, [m]() -> void {});
    }

    void get_statistics()
    {
        auto m = std::make_shared<asionet::message<MsgTypes>>();

        m->m_header.m_id = MsgTypes::Statistics;

        m_intf->send(m, [m]() -> void {});
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
        }

        if ((m_key[Quit] && !m_old_key[Quit]))
        {
            constexpr auto s1 = asionet_make_encrypted_string("Quitting!");
            std::cout << std::string(s1) << "\n";
            return false;
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
                    constexpr auto s1 = asionet_make_encrypted_string("Flood ping [ON]");
                    std::cout << std::string(s1) << "\n";
                }
                else
                {
                    constexpr auto s1 = asionet_make_encrypted_string("Flood ping [OFF]");
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
                m_auto_repeat[Move] = false;
                m_last_transition = std::chrono::system_clock::now();
                move_player(12.0f, 52.0f);
            }
            else if(!m_auto_repeat[Move] && 
                    (m_key[Move] && m_old_key[Move]) && 
                    (deltams > 500ms))
            {
                m_auto_repeat[Move] = true;
            }
            else if(m_auto_repeat[Move] && !m_key[Move] && !m_old_key[Move])
            {
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
    asio::io_context                                m_context;
    std::unique_ptr<interface_type>                 m_intf;
    enum {
        ConnectionRequested,
        ConnectionMade,
        ConnectionWaiting,
        ConnectionComplete
    }                                               m_state{ ConnectionRequested };
    std::vector<uint32_t>                           m_ping_times;
    bool                                            m_flood_ping{ false };
    std::array<bool, 5>                             m_key{};
    std::array<bool, 5>                             m_old_key{};
    bool                                            m_run{ true };
    std::chrono::system_clock::time_point           m_last_transition;
    std::array<bool, 5>                             m_auto_repeat{};
    bool                                            m_ping_inflight{ false };
    std::mutex                                      m_mutex{};

    std::map<MsgTypes, apifunc_type> m_apis = {
    { MsgTypes::Invalid, [](sess_type s, asionet::message<MsgTypes>& m)
                            {                    
                                constexpr auto s1 = asionet_make_encrypted_string("Invalid message received");

                                std::cerr << std::string(s1) << "\n";
                            }
    },
    { MsgTypes::Connected, [this](sess_type s, asionet::message<MsgTypes>& m)
                            {
                                // The server knows that I know that the server knows that I'm connected
                                constexpr auto s1 = asionet_make_encrypted_string("Connected");
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
                                    constexpr auto s1 = asionet_make_encrypted_string("Ping round trip average = ");
                                    constexpr auto s2 = asionet_make_encrypted_string("us (calculated over ");
                                    constexpr auto s3 = asionet_make_encrypted_string(" samples)");

                                    std::cout << std::string(s1)
                                        << (std::accumulate(m_ping_times.begin(), m_ping_times.end(), 0) / m_ping_times.size())
                                        << std::string(s2) << m_ping_times.size() << std::string(s3) << "\n";
                                    m_ping_times.clear();
                                }

                                m_ping_inflight = false;
                            }
    },
    { MsgTypes::FireBullet, [](sess_type s, asionet::message<MsgTypes>& m)
                            {
                                constexpr auto s1 = asionet_make_encrypted_string("response id = ");
                                constexpr auto s2 = asionet_make_encrypted_string(" body is [");
                                constexpr auto s3 = asionet_make_encrypted_string("]");

                                std::cout << std::string(s1) << (uint32_t)m.m_header.m_id << std::string(s2) << (char*)m.m_body.data() << std::string(s3) << "\n";
                            }
    },
    { MsgTypes::MovePlayer, [](sess_type s, asionet::message<MsgTypes>& m)
                            {
                                constexpr auto s1 = asionet_make_encrypted_string("response id = ");
                                constexpr auto s2 = asionet_make_encrypted_string(" body is [");
                                constexpr auto s3 = asionet_make_encrypted_string("]");

                                std::cout << std::string(s1) << (uint32_t)m.m_header.m_id << std::string(s2) << (char*)m.m_body.data() << std::string(s3) << "\n";
                            }
    },
    { MsgTypes::Statistics, [](sess_type s, asionet::message<MsgTypes>& m)
                            {
                                constexpr auto s1 = asionet_make_encrypted_string("Peak sessions: ");
                                constexpr auto s2 = asionet_make_encrypted_string("Peak messages: ");
                                constexpr auto s3 = asionet_make_encrypted_string("Total rx good: ");
                                constexpr auto s4 = asionet_make_encrypted_string("Total rx bad : ");
                                asionet::stats stat;
                                
                                m >> stat;

                                std::cout << std::string(s1) << stat.peak_.sessions_ << "\n"
                                          << std::string(s2) << stat.peak_.msgs_ << "\n"
                                          << std::string(s3) << stat.count_.msgs_rx_good_ << "\n"
                                          << std::string(s4) << stat.count_.msgs_rx_bad_ << "\n";
                            }
    }
    };

 };

int main(int argc, char** argv)
{
    client c;
    
    try
    {
        c.connect(argv[1], atoi(argv[2]), 10s);

        while (c.run()) 
        {
            if(!c.in_flood_ping())
            {
                std::this_thread::sleep_for(10ms);
            }
        };
    }
    catch (std::exception& e)
    {
        std::cout << "Error: " << e.what() << "\n";
    }

    std::terminate();

    return 0;
}
