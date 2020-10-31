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
                m_msg_in_flight[Fire] = false;
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
                m_msg_in_flight[Move] = false;
            }
            else if(m_key[Move] && m_auto_repeat[Move])
            {
                move_player(12.0f, 52.0f);
            }

            break;
        }

        for (auto i = 0; i < sizeof(m_key) / sizeof(m_key[Ping]); ++i) m_old_key[i] = m_key[i];

        return m_run;
    }

private:
    asio::io_context                    m_context;
    std::unique_ptr<interface_type>          m_intf;
    enum {
        ConnectionRequested,
        ConnectionMade,
        ConnectionWaiting,
        ConnectionComplete
    }                                               m_state{ ConnectionRequested };
    std::vector<uint32_t>                           m_ping_times;
    bool                                            m_flood_ping{ false };
    std::array<bool,5>                              m_msg_in_flight;
    std::array<bool,5>                              m_key;
    std::array<bool,5>                              m_old_key;
    bool                                            m_run{ true };
    asionet::protqueue<asionet::message<MsgTypes>>  m_outgoing;
    std::mutex                                      m_state_lock;
    std::chrono::system_clock::time_point           m_last_transition;
    bool                                            m_auto_repeat[3]{false,false,false};

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
                                std::cout << "response id = " << (uint32_t)m.m_header.m_id << " body is [" << (char*)m.m_body.data() << "]\n";
                                m_msg_in_flight[Fire] = false;
                            }
    },
    { MsgTypes::MovePlayer, [this](sess_type s, asionet::message<MsgTypes>& m)
                            {
                                std::cout << "response id = " << (uint32_t)m.m_header.m_id << " body is [" << (char*)m.m_body.data() << "]\n";
                                m_msg_in_flight[Move] = false;
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

 };

int main(int argc, char** argv)
{
    client c;
    
    c.connect(argv[1], atoi(argv[2]));

    while (c.run());

    return 0;
}
