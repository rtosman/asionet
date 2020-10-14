#include <iostream>
#include <asiomsg.hpp>
#include <asioclient.hpp>
#include <ctime>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include "one.hpp"

class Client: public asionet::client_interface<MsgTypes>
{
public:
    void acknowledge_connection()
    {
        asionet::message<MsgTypes> msg;
        msg.m_header.m_id = MsgTypes::Connected;
        send(msg);
    }

    void ping(std::chrono::system_clock::time_point t)
    {
        asionet::message<MsgTypes> msg;
        msg.m_header.m_id = MsgTypes::Ping;
        msg << t;
        send(msg);
    }

    void fire_bullet(float x, float y)
    {
        asionet::message<MsgTypes> msg;
        msg.m_header.m_id = MsgTypes::FireBullet;
        msg << x << y;
        send(msg);
    }

    void move_player(double x, double y)
    {
        asionet::message<MsgTypes> msg;
        msg.m_header.m_id = MsgTypes::MovePlayer;
        msg << x << y;
        send(msg);
    }
 };

int main(int argc, char** argv)
{
    Client c;

    const int Ping=0;
    const int Fire=1;
    const int Move=2;
    const int Quit=3;

    bool key[] = { false, false, false, false };
    bool old_key[] = {false, false, false, false};
    
    c.connect(argv[1], atoi(argv[2]));

    std::vector<uint32_t>       ping_times;
    bool                        flood_ping{ false };
    bool                        ping_in_transit{false};
    bool                        quit{false};
    enum { ConnectionRequested, 
           ConnectionMade,
           ConnectionWaiting, 
           ConnectionComplete}  state{ConnectionRequested};

    while(!quit) {
        if(GetForegroundWindow() == GetConsoleWindow()) 
        {
            key[Ping] = GetAsyncKeyState('P') & 0x8000;
            key[Fire] = GetAsyncKeyState('F') & 0x8000;
            key[Move] = GetAsyncKeyState('M') & 0x8000;
            key[Quit] = GetAsyncKeyState('Q') & 0x8000;
        }

        switch(state)
        {
            case ConnectionRequested:
                if(c.is_connected()) state = ConnectionMade;
            break;
            case ConnectionMade:
                c.acknowledge_connection();
                state = ConnectionWaiting;
            break;
            case ConnectionWaiting:
            break;
            case ConnectionComplete:
                if(key[Ping] && !old_key[Ping])
                {
                    flood_ping = !flood_ping;
                }
                if(key[Fire] && !old_key[Fire])
                {
                    c.fire_bullet(2.0f, 5.0f);
                }
                if(key[Move] && !old_key[Move])
                {
                    c.move_player(12.0f, 52.0f);
                }
            break;
        }

        if((key[Quit] && !old_key[Quit])) 
        {
            std::cout << "Quitting!\n";
            quit = true;
        }

        for(auto i=0; i < sizeof(key)/sizeof(key[0]); ++i) old_key[i] = key[i];

        if (c.is_connected())
        {
            if (flood_ping && !ping_in_transit)
            {
                c.ping(std::chrono::system_clock::now());
                ping_in_transit = true;
            }

            if (!c.incoming().empty())
            {
                auto msg = c.incoming().pop_front().m_msg;

                switch (msg.m_header.m_id)
                {
                case MsgTypes::Connected:
                    std::cout << "Connected to: " << argv[1] << " on port: " << argv[2] << "\n";
                    state = ConnectionComplete;
                break;
                case MsgTypes::Ping:
                {
                    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
                    std::chrono::system_clock::time_point ts;
                    msg >> ts;
                    auto deltaus = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::duration<double>(now - ts));
                    ping_times.push_back(static_cast<uint32_t>(deltaus.count()));
                    if(ping_times.size() >= 1000)
                    {
                        std::cout << "Ping round trip average = " 
                                  << (std::accumulate(ping_times.begin(), ping_times.end(), 0)/ping_times.size()) 
                                  << "us (calculated over " << ping_times.size() << " samples)\n";
                        ping_times.clear();
                    }
                    ping_in_transit = false;
                }
                break;

                case MsgTypes::FireBullet:
                {
                    std::cout << "response id = " << (uint32_t)msg.m_header.m_id << " body is [" << (char*)msg.m_body.data() << "]\n";
                }
                break;

                case MsgTypes::MovePlayer:
                {
                    std::cout << "response id = " << (uint32_t)msg.m_header.m_id << " body is [" << (char*)msg.m_body.data() << "]\n";
                }
                break;
                case MsgTypes::Invalid:
                case MsgTypes::NumEnumElements:
                break;
                }
            }
        }
    }    

    return 0;
}
