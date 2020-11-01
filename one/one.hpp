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
    CaptureTheFlag,
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