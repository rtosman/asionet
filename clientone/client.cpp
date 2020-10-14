#include <iostream>
#include <asiomsg.hpp>
#include <asioclient.hpp>
#include <ctime>
#include <iomanip>
#include <algorithm>
#include <numeric>

enum class MsgTypes: uint32_t
{
    Ping,
    FireBullet,
    MovePlayer
};

class Client: public asionet::client_interface<MsgTypes>
{
public:

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

    bool key[] = { false, false, false, false };
    bool old_key[] = {false, false, false, false};
    
    if(!c.connect(argv[1], atoi(argv[2])))
    {
        std::cout << "Could not connect to: " << argv[1] << " on port: " << argv[2] << "\n";
        exit(1);
    }

    std::cout << "Connected to: " << argv[1] << " on port: " << argv[2] << "\n";

    std::vector<uint32_t>   ping_times;
    bool                    flood_ping{ false };
    bool                    pinging{false};
    bool                    quit{false};
    while(!quit) {
        if(GetForegroundWindow() == GetConsoleWindow()) 
        {
            key[0] = GetAsyncKeyState('P') & 0x8000;
            key[1] = GetAsyncKeyState('F') & 0x8000;
            key[2] = GetAsyncKeyState('M') & 0x8000;
            key[3] = GetAsyncKeyState('Q') & 0x8000;
        }

        if(key[0] && !old_key[0])
        {
            flood_ping = !flood_ping;
        }

        if(key[1] && !old_key[1])
        {
            c.fire_bullet(2.0f, 5.0f);
        }

        if(key[2] && !old_key[2])
        {
            c.move_player(12.0f, 52.0f);
        }

        if((key[3] && !old_key[3]) || !c.is_connected()) 
        {
            std::cout << "Quiting!\n";
            quit = true;
        }

        for(auto i=0; i < sizeof(key); ++i) old_key[i] = key[i];

        if (c.is_connected())
        {
            if (flood_ping && !pinging)
            {
                c.ping(std::chrono::system_clock::now());
                pinging = true;
            }

            if (!c.incoming().empty())
            {
                auto msg = c.incoming().pop_front().m_msg;

                switch (msg.m_header.m_id)
                {
                case MsgTypes::Ping:
                {
                    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
                    std::chrono::system_clock::time_point ts;
                    msg >> ts;
                    auto deltaus = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::duration<double>(now - ts));
                    ping_times.push_back(deltaus.count());
                    if(ping_times.size() >= 1000)
                    {
                        std::cout << "Ping round trip average = " 
                                  << (std::accumulate(ping_times.begin(), ping_times.end(), 0)/ping_times.size()) 
                                  << "us (calculated over " << ping_times.size() << " samples)\n";
                        ping_times.clear();
                    }
                    pinging = false;
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
                }

            }
        }
        else 
        {
            std::cerr << "Not connected to: " << argv[1] << "\n";
        }
    }    

    return 0;
}
