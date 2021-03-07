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


#endif