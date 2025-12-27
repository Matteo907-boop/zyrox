#ifndef RANDOM_HPP
#define RANDOM_HPP

#include <random>

class Random
{
  public:
    template <typename T> static T IntRanged(T min, T max)
    {
        static std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<T> dist(min, max);
        return dist(rng);
    }

    static uint32_t UInt32() { return IntRanged<uint32_t>(0, UINT32_MAX); }

    static uint64_t UInt64() { return IntRanged<uint64_t>(0, UINT64_MAX); }

    static bool Chance(int percent_success)
    {
        return IntRanged<int>(1, 100) <= percent_success;
    }

    class SimpleRNG
    {
        uint32_t m_state;

      public:
        explicit SimpleRNG(uint32_t seed);

        uint32_t Next();

        void Seed(uint32_t seed);
    };
};

#endif // RANDOM_HPP