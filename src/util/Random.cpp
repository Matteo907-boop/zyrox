#include <utils/Random.h>

Random::SimpleRNG::SimpleRNG(uint32_t seed) : m_state(seed) {}

uint32_t Random::SimpleRNG::Next()
{
    m_state += 0x9E3779B9;
    uint32_t z = m_state;
    z ^= z >> 15;
    z *= 0x85EBCA6B;
    z ^= z >> 13;
    z *= 0xC2B2AE35;
    z ^= z >> 16;
    return z;
}

void Random::SimpleRNG::Seed(uint32_t seed) { m_state = seed; }
