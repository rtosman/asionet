#include <iostream>
#include <asiomsg.hpp>
#include <asioclient.hpp>
#include <ctime>
#include <iomanip>

enum class MsgTypes: uint32_t
{
    Ping,
    FireBullet,
    MovePlayer
};

class Client: public asionet::client_interface<MsgTypes>
{
public:

    [[nodiscard]] bool ping(std::chrono::system_clock::time_point t)
    {
        asionet::message<MsgTypes> msg;
        msg.m_header.m_id = MsgTypes::Ping;
        msg << t;
        return m_connection->send(msg);
    }

    [[nodiscard]] bool fire_bullet(float x, float y)
    {
        asionet::message<MsgTypes> msg;
        msg.m_header.m_id = MsgTypes::FireBullet;
        msg << x << y;
        return m_connection->send(msg);
    }

    [[nodiscard]] bool move_player(double x, double y)
    {
        asionet::message<MsgTypes> msg;
        msg.m_header.m_id = MsgTypes::MovePlayer;
        msg << x << y;
        return m_connection->send(msg);
    }

    [[nodiscard]] std::shared_ptr<asionet::message<MsgTypes>> get_response()
    {
        return m_connection->response();
    }
};

int main(int argc, char** argv)
{
    Client c;
    if (c.connect(argv[1], atoi(argv[2])))
    {
        std::cout << "Connected to: " << argv[1] << " on port: " << argv[2] << "\n";
        if (!c.ping(std::chrono::system_clock::now()))
        {
            std::cout << "Could not ping\n";
        }
        else 
        {
            std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
            std::chrono::system_clock::time_point ts;
            auto response = c.get_response();
            *response >> ts;
            auto deltaus = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::duration<double>(now - ts));
            std::cout << "Ping round trip = " << deltaus.count() << "us\n";
        }
        if (!c.fire_bullet(2.0f, 5.0f)) 
        {
            std::cout << "Could not fire bullet\n";
        }
        else 
        {
            auto response = c.get_response();
            std::cout << "response id = " << (uint32_t)response->m_header.m_id << " body is [" << (char*)response->m_body.data() << "]\n";
        }

        if (!c.move_player(80.0, 45.0)) 
        {
            std::cout << "Could not move player\n";
        }
        else
        {
            auto response = c.get_response();
            std::cout << "response id = " << (uint32_t)response->m_header.m_id << " body is [" << (char*)response->m_body.data() << "]\n";
        }
    }
    else 
    {
        std::cerr << "Could not connect to: " << argv[1] << "\n";
    }
    
    system("pause");

    return 0;
}
