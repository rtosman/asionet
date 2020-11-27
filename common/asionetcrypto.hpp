#ifndef _ASIOCRYPTO_HPP_INCLUDED
#define _ASIOCRYPTO_HPP_INCLUDED
#include <botan/auto_rng.h>
#include <botan/cipher_mode.h>
#include <botan/hex.h>

const int     AESBlockSize = 16;

template <uint8_t BlockSize = AESBlockSize>
uint32_t crypto_align(uint32_t size)
{
    if(size == 0) return 0;
    return size + (BlockSize - (size % BlockSize));
}

#endif