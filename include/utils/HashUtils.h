#ifndef HASH_UTIL_H
#define HASH_UTIL_H
#include <cstddef>
#include <inttypes.h>

class HashUtils
{

  public:
    static uint64_t SipHash(uint64_t in, uint64_t k0, uint64_t k1, uint64_t v0,
                            uint64_t v1, uint64_t v2, uint64_t v3);

    static const char *SipHashLlvmIR();
};

#endif // HASH_UTIL_H
