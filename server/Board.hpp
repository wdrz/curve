/*
 * Author:   Witold Drzewakowski
 * Date:     2021-05-25
 * University of Warsaw
 */

#ifndef BOARD_HPP
#define BOARD_HPP

#include <unordered_set>
#include <vector>
#include "Event.hpp"

struct Board;
using board_ptr = std::shared_ptr<Board>;

struct pair_hash {
    template <class T1, class T2>
    std::size_t operator() (std::pair<T1, T2> const &pair) const {
        std::size_t h1 = std::hash<T1>()(pair.first);
        std::size_t h2 = std::hash<T2>()(pair.second);
        return h1 ^ h2;
    }
};

struct Board {
    const int max_x;
    const int max_y;
    std::unordered_set<std::pair<int, int>, pair_hash> eaten_pixels;

    // adding elements to this container does not invalidate iterators.
    std::vector<event_ptr> events;

    int players_playing;

    int event_to_broadcast;

    Board(int max_x_p, int max_y_p) :
            max_x(max_x_p),
            max_y(max_y_p),
            players_playing(0),
            event_to_broadcast(0) {}

    bool contains(std::pair<int, int> p) {
        return eaten_pixels.find(p) != eaten_pixels.end();
    }

    void prepareNewGame(int players) {
        eaten_pixels.clear();
        events.clear();
        players_playing = players;
        event_to_broadcast = 0;
    }

};

#endif //BOARD_HPP
