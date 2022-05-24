/*
 * Author:   Witold Drzewakowski
 * Date:     2021-05-25
 * University of Warsaw
 */

#ifndef EVENT_HPP
#define EVENT_HPP

#include <vector>
#include <cstring>
#include <arpa/inet.h>
#include <memory>

#include "../utils.hpp"

struct Event;
using event_ptr = std::shared_ptr<Event>;

struct Event {
    uint32_t total_size;
    char *content;

    /// New game constructor
    Event(  uint32_t event_num,
            const std::vector<std::string> &names,
            uint32_t maxx,
            uint32_t maxy) {

        uint32_t len = 13;

        for (auto &name: names) {
            len += name.size() + 1;
        }

        total_size = len + 8;
        content = new char[total_size];

        put_uint32(content, len);
        put_uint32(content + 4, event_num);
        put_uint8(content + 8, 0);
        put_uint32(content + 9, maxx);
        put_uint32(content + 13, maxy);

        uint32_t ind = 17;
        for (auto &name: names) {
            std::strcpy(content + ind, name.c_str());
            ind += name.size() + 1;
        }

        put_uint32(content + ind, crc32(content, ind));

    }

    /// Pixel constructor
    Event(uint32_t event_num, uint8_t player_number, uint32_t x, uint32_t y) {
        uint32_t len = 14;
        total_size = len + 8;
        content = new char[total_size];

        put_uint32(content, len);
        put_uint32(content + 4, event_num);
        put_uint8(content + 8, 1);
        put_uint8(content + 9, player_number);
        put_uint32(content + 10, x);
        put_uint32(content + 14, y);
        put_uint32(content + 18, crc32(content, 18));

    }

    /// Player eliminated constructor
    Event(uint32_t event_num, uint8_t player_number) {
        uint32_t len = 6;
        total_size = len + 8;
        content = new char[total_size];

        put_uint32(content, len);
        put_uint32(content + 4, event_num);
        put_uint8(content + 8, 2);
        put_uint8(content + 9, player_number);
        put_uint32(content + 10, crc32(content, 10));
    }


    /// Game over constructor
    explicit Event(uint32_t event_num) {
        uint32_t len = 5;
        total_size = len + 8;
        content = new char[total_size];

        put_uint32(content, len);
        put_uint32(content + 4, event_num);
        put_uint8(content + 8, 3);
        put_uint32(content + 9, crc32(content, 9));

    }

    ~Event() {
        delete[] content;
    }

};

#endif //EVENT_HPP
