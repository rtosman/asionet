
#ifndef _ASIOBUILTIN_HPP_INCLUDED
#define _ASIOBUILTIN_HPP_INCLUDED
#include <cstdint>

namespace asionet {
struct stats 
{
    struct peak
    {
        uint64_t sessions_;
        uint64_t msgs_;
    } peak_;
    struct count
    {
        uint64_t msgs_rx_good_;
        uint64_t msgs_rx_bad_;
    } count_;
};
}
#endif
