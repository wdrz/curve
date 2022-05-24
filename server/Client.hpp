/*
 * Author:   Witold Drzewakowski
 * Date:     2021-05-25
 * University of Warsaw
 */

#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <atomic>
#include <utility>
#include <memory>

struct Client;
using client_ptr = std::shared_ptr<Client>;


enum ClientState {
    OBSERVER,
    LOST,
    JOINED,
    PLAYING,
    READY
};

struct Client {
    ClientState state;
    std::string player_name;
    uint64_t session_id;
    time_t last_datagram_time;
    uint8_t last_turn_direction;
    struct sockaddr_in6 addr;

    Client(ClientState state, std::string playerName, uint64_t sessionId, time_t lastDatagramTime,
           uint8_t lastTurnDirection, struct sockaddr_in6 *addr_p) : state(state), player_name(std::move(playerName)), session_id(sessionId),
                                                              last_datagram_time(lastDatagramTime), last_turn_direction(lastTurnDirection), addr() {
        addr = *addr_p;
    }
};


#endif //CLIENT_HPP
