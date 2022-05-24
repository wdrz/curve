/*
 * Author:   Witold Drzewakowski
 * Date:     2021-05-25
 * University of Warsaw
 */

#ifndef PLAYER_HPP
#define PLAYER_HPP

#include <cmath>
#include <utility>
#include <memory>

#include "Board.hpp"
#include "Client.hpp"

struct Player;

using player_ptr = std::shared_ptr<Player>;

struct Player {
    board_ptr board;

    long double pos_x;
    long double pos_y;
    int direction;

    client_ptr client;
    uint8_t player_num;

    Player(client_ptr c, board_ptr b, uint8_t player_num_p) :
            board(std::move(b)),
            client(std::move(c)),
            player_num(player_num_p) {

        pos_x       = -1;
        pos_y       = -1;
        direction   = -1;
    }

    [[nodiscard]] inline std::pair<int, int> getPixel() const {
        return {(int)floorl(pos_x), (int)floorl(pos_y)};
    }

    bool isOnTheBoard() {
        auto p = getPixel();
        return
            p.first >= 0 &&
            p.first < board->max_x &&
            p.second >= 0 &&
            p.second < board->max_y;
    }

    void generateEventPixel() {
        board->eaten_pixels.insert(getPixel());
        board->events.push_back(std::make_shared<Event>(
                board->events.size(),
                player_num,
                pos_x,
                pos_y));
    }

    void generateEventPlayerEliminated() {
        client->state = LOST;
        board->events.push_back(std::make_shared<Event>(
                board->events.size(),
                player_num));
        board->players_playing--;
    }


    void init() {
        pos_x = ((long double) (Random::rand() % board->max_x)) + 0.5;
        pos_y = ((long double) (Random::rand() % board->max_y)) + 0.5;
        direction = int(Random::rand() % 360);

        if (board->contains(getPixel())) {
            generateEventPlayerEliminated();

        } else {
            generateEventPixel();
        }
    }

    /// returns true if player has been eliminated, false o/w.
    bool move(int direction_change) {
        auto pixel = getPixel();
        direction = (direction + direction_change + 360) % 360;
        long double dir = direction;
        dir = dir * M_PI / 180.0;
        pos_x += cosl(dir);
        pos_y += sinl(dir);
        if (pixel == getPixel()) {
            return false;
        }

        if (!isOnTheBoard() || board->contains(getPixel())) {
            generateEventPlayerEliminated();
            return true;
        }

        generateEventPixel();
        return false;
    }

    bool operator <(const Player& p) const {
        return (client->player_name < p.client->player_name);
    }

};

#endif //PLAYER_HPP
