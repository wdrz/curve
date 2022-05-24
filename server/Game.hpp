/*
 * Author:   Witold Drzewakowski
 * Date:     2021-05-25
 * University of Warsaw
 */

#ifndef GAME_HPP
#define GAME_HPP

#include "../utils.hpp"
#include "Board.hpp"
#include "misc.hpp"
#include "Player.hpp"
#include "Client.hpp"
#include "convertions.hpp"
#include "Event.hpp"

#include <vector>

#include <unordered_map>
#include <unordered_set>
#include <algorithm>

constexpr int MAX_DATAGRAM_SIZE = 548;
constexpr int MIN_NUMBER_OF_PLAYERS = 2;
constexpr int MAX_TIME_OF_INACTIVITY = 2;
constexpr int MAX_CLIENTS = 25;


class Game {
private:
public:
    uint32_t game_id;

    std::vector<player_ptr> players;
    board_ptr board;

    enum state_t {GAME_IN_PROGRESS, WAITING_ROOM} state;

    const int turning_speed;

    /// map user_id's (= address:port) to client
    std::unordered_map<std::string, client_ptr> client_map;

    /// set of usernames that are used by others so that new clients cannot
    /// reuse them
    std::unordered_set<std::string> used_usernames;

    int num_non_observers;
    int num_players_ready;



    Game(int turning_speed_p, int max_x_p, int max_y_p) :
            turning_speed(turning_speed_p) {

        board = std::make_shared<Board>(max_x_p, max_y_p);
        game_id = 0;
        state = WAITING_ROOM;
        num_non_observers = 0;
        num_players_ready = 0;


    }

    void initGame() {

        game_id = Random::rand();

        players.clear();
        players.reserve(num_non_observers);
        uint8_t player_num = 0;

        std::vector<std::string> player_names_list;


        std::vector< std::pair<std::string, std::string> > client_ids_temp;
        for (auto &client: client_map) {
            if (client.second->state != OBSERVER) {
                client_ids_temp.emplace_back(client.second->player_name, client.first);
            }
            //client.second->last_turn_direction = 0;
        }
        std::sort(client_ids_temp.begin(), client_ids_temp.end());
        for (auto &client: client_ids_temp) {
            players.push_back(std::make_shared<Player>(client_map[client.second], board, player_num++));
            client_map[client.second]->state = PLAYING;
            player_names_list.emplace_back(client.first);

        }

        state = GAME_IN_PROGRESS;
        board->prepareNewGame(player_num);
        board->events.push_back(std::make_shared<Event>(0, player_names_list, board->max_x, board->max_y));
        num_players_ready = 0;

        for (auto &player: players) {
            player->init();
        }
    }

    bool handleUnrecognisedClient(
            const std::string &client_id,
            const client_mess &mess) {

        if (client_map.size() >= MAX_CLIENTS) {
            // Too many connected clients
            return false;
        }

        if (used_usernames.find(mess.player_name) != used_usernames.end()) {
            // new client tries to impersonate other user, ignore him
            return false;
        }

        for (char c : mess.player_name) {
            if (c < 33 || c > 126) {
                // player name contains incorrect character
                return false;
            }
        }


        client_map.insert({
                  client_id,
                  std::make_shared<Client>(
                          OBSERVER,
                          mess.player_name,
                          mess.session_id,
                          time(nullptr),
                          mess.turn_direction,
                          mess.addr)
        });

        if (!mess.player_name.empty()) {
            client_map[client_id]->state = JOINED;
            used_usernames.emplace(mess.player_name);
            num_non_observers++;
        }

        return true;
    }


    bool handleClient(
            const std::string &client_id,
            const client_mess &mess) {

        auto client = client_map.find(client_id);

        if (client == client_map.end()) {

            if (!handleUnrecognisedClient(client_id, mess))
                return false;

        } else {
            // socket has been recognised
            if (client->second->session_id > mess.session_id) {
                // datagram with lesser session_id, ignore it
#ifdef DEBUG
                std::cout << "Datagram from recognised source with incorrect (lesser) session_id, ignore it" << std::endl;
#endif
                return false;

            }

            else if (client->second->player_name != mess.player_name) {
                // wrong username from connected client, ignore this datagram
#ifdef DEBUG
                std::cout << "wrong username from connected client, ignore this datagram" << std::endl;
#endif
                return false;
            }

            else if (client->second->session_id < mess.session_id) {
                // datagram with greater session_id, disconnect previous client
                // and join as a new one
                client_map.erase(client);

                client_map.insert({
                    client_id,
                    std::make_shared<Client>(
                            JOINED,
                            mess.player_name,
                            mess.session_id,
                            time(nullptr),
                            mess.turn_direction,
                            mess.addr
                            )
                });

            }
            else {
                // session_id and socket recognised
                client->second->last_datagram_time  = time(nullptr);
                client->second->last_turn_direction = mess.turn_direction;
            }
        }


        return true;

    }

    bool waitingRoomRoutine(const std::string &client_id) {
        auto client = client_map.find(client_id);

        switch (client->second->state) {
            case JOINED:
            case LOST:
            case PLAYING:
                if (client->second->last_turn_direction != 0) {
                    client->second->state = READY;
                    num_players_ready++;
                }

                break;

            default:
                ;
        }

        if (num_players_ready == num_non_observers && num_players_ready >= MIN_NUMBER_OF_PLAYERS) {
            initGame();
            return true;
        }
        return false;

    }


    void disconnectInactiveClients() {
        unsigned curr_time = time(nullptr);

        for (auto client_it = client_map.cbegin(); client_it != client_map.cend();) { // iterate and erase idiom

            if (client_it->second->last_datagram_time + MAX_TIME_OF_INACTIVITY < curr_time) {

                if (client_it->second->state != OBSERVER)
                    num_non_observers--;

#ifdef DEBUG
                std::cout << "Disconnecting client " << client_it->second->player_name << std::endl;
#endif
                used_usernames.erase(client_it->second->player_name);
                client_it = client_map.erase(client_it);

            } else {
                ++client_it;
            }
        }

    }

    /// Assumes that buffer is at least MAX_DATAGRAM_SIZE long.
    /// Modifies arguments @p from and @p buffer.
    /// @returns length of message.

    int buildDatagram(unsigned int &from, char *buffer) {
        int len = 4;

        put_uint32(buffer, game_id);

        while ( from < board->events.size() &&
                len + board->events[from]->total_size <= MAX_DATAGRAM_SIZE) {

            std::memcpy(buffer + len, board->events[from]->content, board->events[from]->total_size);

            len += (int) board->events[from]->total_size;

            from++;
        }

        if (len == 4) {
            return 0;
        }

        return len;

    }



    bool doRound() {
        for (auto &player: players) {
            if (player->client->state == PLAYING) {
                switch (player->client->last_turn_direction) {
                    case 0:
                        player->move(0);
                        break;
                    case 1:
                        player->move(turning_speed);
                        break;
                    case 2:
                        player->move(-turning_speed);
                        break;
                }

            }

            // check if the game has ended
            if (board->players_playing <= 1) {
                // generate event game over
                board->events.push_back(std::make_shared<Event>(board->events.size()));
                state = WAITING_ROOM;
                return true;
            }
        }
        return false;
    }

    /**
     * Checks if the current state of the game is waiting for users to start a game.
     * @return      proper logical value.
     */
    bool isWaitingRoom() const {
        return state == WAITING_ROOM;
    }

};


#endif //GAME_HPP
