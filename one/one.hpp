#ifndef _ONE_HPP_INCLUDED
#define _ONE_HPP_INCLUDED
#include <cstdint>

// User specific MsgTypes.  The first enum (0) has to be "Invalid"
// and the last must be NumEnumELements. Enum value 0 is used for
// internal messages and cannot be used, all user messages must be
// 1 or higher
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
struct onetx
{
    uint8_t x;
    uint8_t y;
};

std::ostream& operator<<(std::ostream& o, onetx const& p)
{  
    return o << (int)p.x << ", " << (int)p.y;  
}

constexpr onetx genslider(std::size_t curr, std::size_t total)
{
    return {(uint8_t)(curr*13/(total-1)), (uint8_t)(curr*19/total)};
}

template<typename T, int N>
uint8_t slide(uint8_t in, std::array<T,N> arr)
{
    for (auto p: arr)
    {
        if (p.x == (in&0xf))
        {
            return p.y;
        }
    }

    return 0;
}

#endif