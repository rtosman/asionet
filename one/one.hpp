#ifndef _ONE_HPP_INCLUDED
#define _ONE_HPP_INCLUDED
#include <cstdint>

enum class MsgTypes: uint32_t
{
    Invalid,
    Connected,
    Ping,
    FireBullet,
    MovePlayer,
    Statistics,
    NumEnumElements
};

MsgTypes clamp_msg_types(MsgTypes id)
{
    if((static_cast<uint32_t>(id) > static_cast<uint32_t>(MsgTypes::Invalid)) && 
       (static_cast<uint32_t>(id) < static_cast<uint32_t>(MsgTypes::NumEnumElements)))
    {
        return (MsgTypes)id;
    }
    return MsgTypes::Invalid;
}

// following is the support for the authentication challenge
struct point
{
    uint8_t x;
    uint8_t y;
};

std::ostream& operator<<(std::ostream& o, point const& p)
{  
    return o << (int)p.x << ", " << (int)p.y;  
}

constexpr point genslider(std::size_t curr, std::size_t total)
{
    uint8_t cur = curr;
    uint8_t tot = total;

    return {(uint8_t)(cur*4/(tot-1)), (uint8_t)(cur*2/(tot-1))};
}

template<typename T, int N>
uint8_t slide(uint8_t in, std::array<T,N> arr)
{
    for (auto p: arr)
    {
        if (p.x == in)
        {
            return p.y;
        }
    }

    return 0;
}

#endif