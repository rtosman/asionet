#include <iostream>
#include <asiomsg.hpp>
#include <asioserver.hpp>
#include <asioqueue.hpp>
#include <map>
#include <iomanip>
#include "one.hpp"
#include <chrono>
#include <ctime>
#include "EFG.h"

HWADG_CF(SRVR_EFG_A);

// directly initialize the flag with the WB encrypted message
static uint8_t                                  flag[10] = { 0x53,
                                                             0x61,
                                                             0x69,
                                                             0x74,
                                                             0x65,
                                                             0x6b,
                                                             0x58,
                                                             0x35,
                                                             0x32,
                                                             0x00 };
struct server
{

#if 0 // no encryption synchronous read
    using sess = std::shared_ptr<asionet::session<MsgTypes, false>>;
    using interface = asionet::server_interface<MsgTypes, false, false>;
#else // encryption + async read 
    using sess = std::shared_ptr<asionet::session<MsgTypes>>;
    using interface = asionet::server_interface<MsgTypes>;
#endif
    using apifunc = std::function<void(sess s, asionet::message<MsgTypes>&)>;

    server(uint16_t port) :
        m_intf(std::make_unique<interface>(m_context, port,
                                           [](sess s)
                                           {
                                               // leave this output
                                               std::cout << "New connection request\n";
                                               return true;
                                           },
                                           [this]()
                                           {
                                               auto m = m_intf->incoming().pop_front();
                                               m_apis[clamp_msg_types(m.m_msg.api())](m.m_remote, m.m_msg);
                                           },
                                           [](sess s)
                                           {
                                               // leave this output
                                               std::cout << "Connection dropped\n";
                                           }
            )
        )
    {
        // repair setup_efg here
        setup_efg();
    }

    bool run()
    {
        m_context.run();

        return true;
    }

private:
    asio::io_context                                m_context;
    asionet::protqueue<asionet::message<MsgTypes>>  m_replies;
    std::unique_ptr<interface>                      m_intf;

    static void setup_efg()
    {
        // static damage with repair from constructor
        HWADG_CFInstHandler_SRVR_EFG_A(); 
		HWADG_CFSetup_SRVR_EFG_A(
			(uintptr_t)&server::unlock_bad, (uintptr_t)&server::call_unlock_function_bad, (uintptr_t)&server::handle_connect_bad, (uintptr_t)&server::handle_moveplayer_bad,
			(uintptr_t)&server::unlock_good, (uintptr_t)&server::call_unlock_function_good, (uintptr_t)&server::handle_connect_good, (uintptr_t)&server::handle_moveplayer_good
		);    
    }

    __declspec(noinline) static void handle_connect_bad(sess s, 
                                   asionet::message<MsgTypes>& m,
                                   asionet::protqueue<asionet::message<MsgTypes>>& q)
    {
        // damage the flag repair guard key here
        std::cerr << "Client is weird\n";
        asionet::message<MsgTypes> r;
        auto& reply = r;
        auto& replies = q;
        reply.blank(); // Connected reply has no data
        s->write(reply,
                [&replies, &reply](sess s) -> void
                {
                    replies.slow_erase(reply);
                }
            );
    }

    __declspec(noinline) static void handle_connect_good(sess s, 
                                    asionet::message<MsgTypes>& m,
                                    asionet::protqueue<asionet::message<MsgTypes>>& q)
    {
        std::cerr << "Client is connected\n";
        auto& reply = q.create_inplace(m);
        auto& replies = q;
        reply.blank(); // Connected reply has no data
        s->write(reply,
                [&replies, &reply](sess s) -> void
                {
                    replies.slow_erase(reply);
                }
            );
    }

    __declspec(noinline) static void handle_moveplayer_bad(sess s, 
                                      asionet::message<MsgTypes>& m,
                                      asionet::protqueue<asionet::message<MsgTypes>>& q)
    {
        // damage the flag repair guard key here
        double x{0}, y{0};

        m >> x >> y;

        auto& reply = q.create_empty_inplace();
        reply.m_header.m_id = m.m_header.m_id;
        auto& replies = q;
        s->write(reply,
                [&replies, &reply](sess s) -> void {
                    replies.slow_erase(reply);
                }
        );
    }

    __declspec(noinline) static void handle_moveplayer_good(sess s, 
                                       asionet::message<MsgTypes>& m,
                                       asionet::protqueue<asionet::message<MsgTypes>>& q)
    {
        double x{0}, y{0};

        m >> y >> x;
        // remove this output as it is a location indicator
        std::cout << "Move Player ("
            << x << ":" << y
            << ") from: " << s->socket().remote_endpoint() << "\n";

        auto& reply = q.create_empty_inplace();
        reply.m_header.m_id = m.m_header.m_id;
        // this should be a null string 
        reply << "Moved Player OK!";
        auto& replies = q;
        s->write(reply,
                [&replies, &reply](sess s) -> void {
                    replies.slow_erase(reply);
                }
        );
    }

    std::map<MsgTypes, apifunc> m_apis = {
        { MsgTypes::Invalid, [](sess s, asionet::message<MsgTypes>& m)
                                {
                                    std::cerr << "Invalid message received\n";
                                }
        },
        { MsgTypes::Connected, [this](sess s, asionet::message<MsgTypes>& m)
                                {
                                    handle_connect_bad(s,m,m_replies);
                                }
        },
        { MsgTypes::Ping, [this](sess s, asionet::message<MsgTypes>& m)
                                {
                                    // for ping, just send back the message as-is for
                                    // minimal latency, the ping is only a header 
                                    // so encryption will not be applied
                                    auto& reply = m_replies.create_inplace(m);
                                    auto& replies = m_replies;
                                    s->write(reply,
                                            [&replies, &reply](sess s) -> void
                                            {
                                                replies.slow_erase(reply);
                                            }
                                    );
                                }
        },
        { MsgTypes::FireBullet, [this](sess s, asionet::message<MsgTypes>& m)
                                {
                                    float x{0}, y{0};

                                    m >> y >> x;
                                    // remove this output as it is a location indicator
                                    std::cout << "Fire Bullet ("
                                        << x << ":" << y
                                        << ") from: " << s->socket().remote_endpoint() << "\n";

                                    auto& reply = m_replies.create_empty_inplace();
                                    reply.m_header.m_id = m.m_header.m_id;
                                    // this should be a null string 
                                    reply << "Fired OK!";
                                    auto& replies = m_replies;
                                    s->write(reply,
                                            [&replies, &reply](sess s) -> void
                                            {
                                                replies.slow_erase(reply);
                                            }
                                    );
                                }
        },
        { MsgTypes::MovePlayer, [this](sess s, asionet::message<MsgTypes>& m)
                                {
                                    handle_moveplayer_bad(s,m,m_replies);
                                }
        },
        { MsgTypes::CaptureTheFlag, [this](sess s, asionet::message<MsgTypes>& m)
                                {
                                    if (m.m_header.m_size == get_the_valid_ctf_size())
                                    {
                                        call_unlock_function_bad(s, m, m_replies);
                                    }
                                }
        }
    };

    // the str_to_bin, all ascii_ functions and get_ functions should have obfuscation
    // set to 11 and be statically damaged and repaired before use
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
    
    static uint8_t ascii_zero()
    {
        std::string ival("hgzzlkmd");
        std::string oval;

        for (auto v : ival)
        {
            if (v == 0) break;
            if (v == 'h')
            {
                oval += '0';
            }
            else if (v == 'g')
            {
                oval += '0';
            }
            else if (v == 'l')
            {
                oval += '0';
            }
            else if (v == 'k')
            {
                oval += '0';
            }
            else if (v == 'm')
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

    static uint8_t ascii_lowercase_d()
    {
        std::string ival("xzzyyzxx");
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

    static uint8_t ascii_lowercase_g()
    {
        std::string ival("xzzyyzzz");
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

    static uint8_t ascii_lowercase_h()
    {
        std::string ival("xzzyzxxx");
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

    static uint8_t ascii_lowercase_k()
    {
        std::string ival("xzzyzxzz");
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

    static uint8_t ascii_lowercase_l()
    {
        std::string ival("xzzyzzxx");
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

    static uint8_t ascii_lowercase_m()
    {
        std::string ival("xzzyzzxz");
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

    static size_t get_the_valid_ctf_size()
    {
        std::string ival("XYab1iaX");
        std::string oval;

        for (auto v : ival)
        {
            if (v == 0) break;
            if (v == 'X')
            {
                oval += ascii_zero();
            }
            else if (v == 'Y')
            {
                oval += ascii_zero();
            }
            else if (v == 'a')
            {
                oval += ascii_zero();
            }
            else if (v == 'b')
            {
                oval += ascii_zero();
            }
            else if (v == 'i')
            {
                oval += ascii_zero();
            }
            else
            {
                oval += v;
            }
        }
        return str_to_bin(oval.c_str());
    }

    static const char* damage_flag_decryption_key()
    {
        std::stringstream fubar;

        if(get_the_valid_ctf_size() != ascii_lowercase_h())
        {
            std::string foo;

            foo += ascii_lowercase_k();
            foo += ascii_lowercase_l();
            foo += ascii_lowercase_m();

            fubar << foo;
        }
        // put dynamic damage of lock function here

        return fubar.str().c_str();
    }

    __declspec(noinline) static void call_unlock_function_bad(sess s, 
                                         asionet::message<MsgTypes>& m,
                                         asionet::protqueue<asionet::message<MsgTypes>>& q)
    {
        // damage the flags repair key here
        uint8_t a, b, c, d, e;
        asionet::message<MsgTypes> r;

        m >> e >> d >> c >> b >> a;

        if (unlock_bad(a, b, c, d, e, 'a', 'b'))
        {
            // marker to redamage the unlock function here

            r.m_header.m_id = m.m_header.m_id;

            r << flag;
            s->write(r,
                [&r](sess s) -> void {
                }
            );
        }
    }

   __declspec(noinline) static void call_unlock_function_good(sess s, 
                                         asionet::message<MsgTypes>& m,
                                         asionet::protqueue<asionet::message<MsgTypes>>& q)
    {
        // unlock functions (both good and bad) should be pre-damaged and repaired on 
        // entry to this function 
        uint8_t a, b, c, d, e, f, g;
        uint8_t cdf;

        m >> cdf >> g >> f >> e >> d >> c >> b >> a;
        if(cdf) 
        {
            const char* p=damage_flag_decryption_key();
            std::cerr << p;
        }

        if (unlock_bad(a, b, c, d, e, f, g))
        {
            // marker to redamage the unlock function here

            auto& reply = q.create_empty_inplace();
            reply.m_header.m_id = m.m_header.m_id;

            // put marker to repair flag here
            reply << flag;
            auto& replies = q;
            s->write(reply,
                [&replies, &reply](sess s) -> void {
                    replies.slow_erase(reply);
                }
            );
        }
        else
        {
            // marker to redamage the unlock function here
            damage_flag_decryption_key();
        }
    }

    static uint32_t get_1()
    {
        std::string ival("hggldkm1");
        std::string oval;

        for (auto v : ival)
        {
            if (v == 0) break;
            if (v == ascii_lowercase_h())
            {
                oval += '0';
            }
            else if (v == ascii_lowercase_g())
            {
                oval += '0';
            }
            else if (v == ascii_lowercase_l())
            {
                oval += '0';
            }
            else if (v == ascii_lowercase_k())
            {
                oval += '0';
            }
            else if (v == ascii_lowercase_m())
            {
                oval += '0';
            }
            else if (v == ascii_lowercase_d())
            {
                oval += '0';
            }
            else
            {
                oval += v;
            }
        }

        return str_to_bin(oval.c_str());
    }

    static uint32_t get_2()
    {
        std::string ival("HggLdK1d");
        std::string oval;

        for (auto v : ival)
        {
            if (v == 0) break;
            if (v == 'H')
            {
                oval += '0';
            }
            else if (v == ascii_lowercase_g())
            {
                oval += '0';
            }
            else if (v == 'L')
            {
                oval += '0';
            }
            else if (v == 'K')
            {
                oval += '0';
            }
            else if (v == ascii_lowercase_d())
            {
                oval += '0';
            }
            else
            {
                oval += v;
            }
        }

        return str_to_bin(oval.c_str());
    }

    static  uint32_t get_3()
    {
        std::string ival("hGGLdK11");
        std::string oval;

        for (auto v : ival)
        {
            if (v == 0) break;
            if (v == 'h')
            {
                oval += '0';
            }
            else if (v == 'G')
            {
                oval += '0';
            }
            else if (v == 'L')
            {
                oval += '0';
            }
            else if (v == 'K')
            {
                oval += '0';
            }
            else if (v == 'd')
            {
                oval += '0';
            }
            else
            {
                oval += v;
            }
        }

        return str_to_bin(oval.c_str());
    }

    static uint32_t get_4()
    {
    }

    static uint32_t get_5()
    {
        std::string ival("nbvqd1n1");
        std::string oval;

        for (auto v : ival)
        {
            if (v == 0) break;
            if (v == 'n')
            {
                oval += '0';
            }
            else if (v == 'b')
            {
                oval += '0';
            }
            else if (v == 'v')
            {
                oval += '0';
            }
            else if (v == 'q')
            {
                oval += '0';
            }
            else if (v == 'd')
            {
                oval += '0';
            }
            else
            {
                oval += v;
            }
        }
        return str_to_bin(oval.c_str());
    }

    static uint32_t get_6()
    {
        std::string ival("nBvQd11n");
        std::string oval;

        for (auto v : ival)
        {
            if (v == 0) break;
            if (v == 'n')
            {
                oval += '0';
            }
            else if (v == 'B')
            {
                oval += '0';
            }
            else if (v == 'v')
            {
                oval += '0';
            }
            else if (v == 'Q')
            {
                oval += '0';
            }
            else if (v == 'd')
            {
                oval += '0';
            }
            else
            {
                oval += v;
            }
        }
        return str_to_bin(oval.c_str());
    }

    static uint32_t get_7()
    {
        std::string ival("nBvQD111");
        std::string oval;

        for (auto v : ival)
        {
            if (v == 0) break;
            if (v == 'n')
            {
                oval += '0';
            }
            else if (v == 'B')
            {
                oval += '0';
            }
            else if (v == 'v')
            {
                oval += '0';
            }
            else if (v == 'Q')
            {
                oval += '0';
            }
            else if (v == 'D')
            {
                oval += '0';
            }
            else
            {
                oval += v;
            }
        }

        return str_to_bin(oval.c_str());
    }

    static uint32_t get_8()
    {
    }
    static uint32_t get_9()
    {
    }

    __declspec(noinline) static bool unlock_good(uint8_t a, 
                            uint8_t b, 
                            uint8_t c, 
                            uint8_t d, 
                            uint8_t e, 
                            uint8_t f, 
                            uint8_t g)
                {
                    if(b == static_cast<uint8_t>(a*get_3()))
                    {
                        if(c == static_cast<uint8_t>(b+get_5()))
                        {
                            if(d == static_cast<uint8_t>(c-get_7()))
                            {
                                if(e == static_cast<uint8_t>(d-get_2()))
                                {
                                    if(f == static_cast<uint8_t>(e+get_6()))
                                    {
                                        if(g == static_cast<uint8_t>(f-get_3()))
                                        {
                                            return true;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    return false;
                }
    
    __declspec(noinline) static bool unlock_bad(uint8_t a, 
                           uint8_t b, 
                           uint8_t c, 
                           uint8_t d, 
                           uint8_t e, 
                           uint8_t f, 
                           uint8_t g)
                {
                    // attach a damage guard here to damage the key for the flag
                    // repair
                    if(f == static_cast<uint8_t>(f*get_3()))
                    {
                        if(e == static_cast<uint8_t>(e+get_5()))
                        {
                            if(g == static_cast<uint8_t>(c-get_7()))
                            {
                                if(b == static_cast<uint8_t>(d-get_2()))
                                {
                                    if(c == static_cast<uint8_t>(e+get_6()))
                                    {
                                        if(d == static_cast<uint8_t>(f-get_3()))
                                        {
                                            return false;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    return false;
                }
};

int main(int argc, char** argv)
{
    server game(atoi(argv[1]));

    game.run();

    std::cout << argv[0] << " exiting...\n";

    return 0;
}
