#include <iostream>
#include <asiomsg.hpp>
#include <asioclient.hpp>
#include <ctime>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <map>
#include "one.hpp"

struct client
{
    using sess = asionet::client_interface<MsgTypes>::sess;
    using interface = asionet::client_interface<MsgTypes>;
    using apifunc = std::function<void(sess s, asionet::message<MsgTypes>&)>;

    client():
        m_intf(std::make_unique<interface>(m_context,
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
        m_intf->send(msg, [this, &msg]()
            {
                remove_sent(msg);
            });
    }

    void ping(std::chrono::system_clock::time_point t)
    {
        std::scoped_lock lock(m_state_lock);

        if (m_ping_in_transit)
        {
            return;
        }

        auto& msg = m_outgoing.create_empty_inplace();

        msg.m_header.m_id = MsgTypes::Ping;
        msg << t;
        m_intf->send(msg, [this, &msg]()
            {
                remove_sent(msg);
            });

        m_ping_in_transit = true;
    }

    void fire_bullet(float x, float y)
    {
        auto& msg = m_outgoing.create_empty_inplace();

        msg.m_header.m_id = MsgTypes::FireBullet;
        msg << x << y;
        m_intf->send(msg, [this, &msg]()
            {
                remove_sent(msg);
            });
    }

    void move_player(double x, double y)
    {
        auto& msg = m_outgoing.create_empty_inplace();

        msg.m_header.m_id = MsgTypes::MovePlayer;
        msg << x << y;
        m_intf->send(msg, [this, &msg]() 
                        {
                remove_sent(msg);
                        }
                    );
    }

    bool run()
    {
        const int Ping = 0;
        const int Fire = 1;
        const int Move = 2;
        const int Quit = 3;


        if (GetForegroundWindow() == GetConsoleWindow())
        {
            m_key[Ping] = GetAsyncKeyState('P') & 0x8000;
            m_key[Fire] = GetAsyncKeyState('F') & 0x8000;
            m_key[Move] = GetAsyncKeyState('M') & 0x8000;
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
                fire_bullet(2.0f, 5.0f);
            }
            if (m_key[Move] && !m_old_key[Move])
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
    std::unique_ptr<interface>          m_intf;
    enum {
        ConnectionRequested,
        ConnectionMade,
        ConnectionWaiting,
        ConnectionComplete
    }                                               m_state{ ConnectionRequested };
    std::vector<uint32_t>                           m_ping_times;
    bool                                            m_flood_ping{ false };
    bool                                            m_ping_in_transit{ false };
    std::array<bool,4>                              m_key{false, false, false, false};
    std::array<bool, 4>                             m_old_key{false, false, false, false};
    bool                                            m_run{ true };
    asionet::protqueue<asionet::message<MsgTypes>>  m_outgoing;
    std::mutex                                      m_state_lock;

    std::map<MsgTypes, apifunc> m_apis = {
    { MsgTypes::Invalid, [](sess s, asionet::message<MsgTypes>& m)
                            {
                                std::cerr << "Invalid message received\n";
                            }
    },
    { MsgTypes::Connected, [this](sess s, asionet::message<MsgTypes>& m)
                            {
                                std::cout << "Connected \n";
                                m_state = ConnectionComplete;
                            }
    },
    { MsgTypes::Ping, [this](sess s, asionet::message<MsgTypes>& m)
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
                                m_ping_in_transit = false;
                            }
    },
    { MsgTypes::FireBullet, [](sess s, asionet::message<MsgTypes>& m)
                            {
                                std::cout << "response id = " << (uint32_t)m.m_header.m_id << " body is [" << (char*)m.m_body.data() << "]\n";
                            }
    },
    { MsgTypes::MovePlayer, [](sess s, asionet::message<MsgTypes>& m)
                            {
                                std::cout << "response id = " << (uint32_t)m.m_header.m_id << " body is [" << (char*)m.m_body.data() << "]\n";
                            }
    }
    };

    void remove_sent(asionet::message<MsgTypes>& msg)
    {
        for (auto i = m_outgoing.begin(); i != m_outgoing.end(); )
        {
            (&(*i) == &msg) ? i = m_outgoing.erase(i) : ++i;
        }
    }
 };

int main(int argc, char** argv)
{
    client c;
    
    c.connect(argv[1], atoi(argv[2]));

    while (c.run());

    return 0;
}
