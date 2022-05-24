/*
 * Author:   Witold Drzewakowski
 * Date:     2021-05-25
 * University of Warsaw
 */

#ifndef MISC_HPP
#define MISC_HPP

#include <vector>
#include <string>

inline std::string toSocket(const std::string &addr, uint16_t port) {
    return addr + ":" + std::to_string(port);
}

class Random {
    static uint64_t value;
public:
    static void set(uint32_t seed);
    static uint32_t rand();
};

#endif //MISC_HPP
