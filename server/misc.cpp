/*
 * Author:   Witold Drzewakowski
 * Date:     2021-05-25
 * University of Warsaw
 */

#include <cstdint>
#include "misc.hpp"

uint64_t Random::value = 123456789; // irrelevant number will be overwritten

void Random::set(uint32_t seed) {
    Random::value = (int64_t)seed;
}

uint32_t Random::rand() {
    uint32_t sol = Random::value;
    Random::value = (Random::value * 279410273) % 4294967291;
    return sol;
}
