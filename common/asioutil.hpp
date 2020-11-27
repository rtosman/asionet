#ifndef _ASIOUTIL_HPP_INCLUDED
#define _ASIOUTIL_HPP_INCLUDED
#include "asionet.hpp"
#include <list>
#include <map>
#include <optional>
#include <functional>

void set_result(std::optional<asio::error_code>* a, asio::error_code b ) 
{
    *a = b;
}

template<typename MutableBufferSequence>
std::tuple<std::optional<asio::error_code>, std::optional<asio::error_code>> 
read_with_timeout(
            asio::io_context& ctxt,
            asio::ip::tcp::socket& sock,
            const MutableBufferSequence& buffer,
            const std::chrono::seconds& tmout
)
{
    std::optional<asio::error_code> timer_result;
    std::optional<asio::error_code> read_result;

    asio::steady_timer timer(ctxt);
    
    timer.expires_from_now(tmout);
    timer.async_wait(std::bind(set_result, &timer_result, std::placeholders::_1));
    
    asio::async_read(
                    sock,
                    buffer,
                    std::bind(set_result,
                              &read_result,
                              std::placeholders::_1
                             )
        );
      
    ctxt.reset();
    do
    {
        ctxt.run_one_for(100ms);

        if (read_result)
        {
            timer.cancel();
            break;
        }
        else if (timer_result)
        {
            sock.cancel();
            break;
        }
    } while (true);

    return std::make_tuple(read_result, timer_result);
}
#endif