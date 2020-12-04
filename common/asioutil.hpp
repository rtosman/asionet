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

// by Xeo, from https://stackoverflow.com/a/13294458/420683
template<std::size_t... Is> struct seq{};
template<std::size_t N, std::size_t... Is>
struct gen_seq : gen_seq<N-1, N-1, Is...>{};
template<std::size_t... Is>
struct gen_seq<0, Is...> : seq<Is...>{};

template<class Generator, std::size_t... Is>
constexpr auto generate_array_helper(Generator g, seq<Is...>)
-> std::array<decltype(g(std::size_t{}, sizeof...(Is))), sizeof...(Is)>
{
    return {{g(Is, sizeof...(Is))...}};
}

template<std::size_t tcount, class Generator>
constexpr auto generate_array(Generator g)
-> decltype( generate_array_helper(g, gen_seq<tcount>{}) )
{
    return generate_array_helper(g, gen_seq<tcount>{});
}

#endif