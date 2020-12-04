#ifndef _ASIOUTIL_HPP_INCLUDED
#define _ASIOUTIL_HPP_INCLUDED
#include "asionet.hpp"
#include <list>
#include <map>
#include <optional>
#include <functional>

constexpr auto steady_min = std::chrono::steady_clock::time_point::min();

struct tmr_data
{
    tmr_data(asio::steady_timer& tmr, std::optional < asio::error_code>* a_val, std::optional < asio::error_code>* r_val) : t(tmr), a(a_val), r(r_val) {}

    asio::steady_timer& t;
    std::optional<asio::error_code>* a;
    std::optional<asio::error_code>* r;
};

void tmr_handler(tmr_data& td, asio::error_code b) 
{
    if (!(*td.r) && (td.t.expires_at() != steady_min)) {
        *(td.a) = b;
    }
}

void read_hander(std::optional<asio::error_code>* a, asio::error_code b) 
{
    *a = b;
}

template<typename MutableBufferSequence>
std::tuple<std::optional<asio::error_code>, std::optional<asio::error_code>>
read_with_timeout(asio::ip::tcp::socket& sock, 
                  asio::io_context& ctxt,
                  asio::io_context::strand& read,
                  const MutableBufferSequence& buffer,
                  const std::chrono::seconds& tmout
)
{
    std::optional<asio::error_code> timer_result;
    std::optional<asio::error_code> read_result;

    ctxt.reset();

    asio::steady_timer timer(ctxt);

    tmr_data td(timer, &timer_result, &read_result);

    timer.expires_from_now(tmout);
    timer.async_wait(asio::bind_executor(read,
        std::bind(tmr_handler, td, std::placeholders::_1)
    )
    );
        
    asio::async_read(sock,
        buffer,
        asio::bind_executor(read,
            std::bind(read_hander,
                &read_result,
                std::placeholders::_1
            )
        )
    );

    do
    {
        ctxt.run_for(100ms);
        if (read_result)
        {
            read.post([&timer]()
                {
                    timer.expires_at(steady_min);
                }
            );
            timer.cancel();
            ctxt.run_for(100ms);
            break;
        }
        else if (timer_result)
        {
            sock.cancel();
            break;
        }
    } while (true);

    std::this_thread::sleep_for(10ms);

    return std::make_tuple(read_result, timer_result);
}

template<typename MutableBufferSequence>
std::tuple<std::optional<asio::error_code>, std::optional<asio::error_code>> 
read_with_timeout(asio::ip::tcp::socket& sock,
                  const MutableBufferSequence& buffer,
                  const std::chrono::seconds& tmout
                 )
{
    asio::io_context ctxt;
    asio::io_context::strand read(ctxt);

    return read_with_timeout(sock, ctxt, read, buffer, tmout);
}

std::tuple<std::shared_ptr<uint8_t>, size_t> init_challenge(uint8_t* space, size_t amount)
{
    std::shared_ptr<uint8_t> copy(new uint8_t[amount]);

    Botan::AutoSeeded_RNG rng;
    Botan::secure_vector<uint8_t> random_data = rng.random_vec(amount);
    std::memcpy(space, random_data.data(), random_data.size());
    std::memcpy(copy.get(), space, random_data.size());
    return std::make_tuple(copy, random_data.size());
}

template<typename T>
unsigned char verify_response(std::shared_ptr<asionet::session<T, true>> s,
    uint8_t* resp,
    const std::tuple<std::shared_ptr<uint8_t>, size_t>& answer)
{
    Botan::secure_vector<uint8_t> encrypted = s->encrypt(std::get<0>(answer).get(), std::get<1>(answer));
    return memcmp(&resp[0], encrypted.data() + 4, std::get<1>(answer));
}

#endif