/*
 * Author:   Witold Drzewakowski
 * Date:     2021-05-25
 * University of Warsaw
 */

#ifndef CLIENT_STATE_HPP
#define CLIENT_STATE_HPP

#include <string>
#include <utility>
#include <cstring>
#include <queue>
#include <arpa/inet.h>

#include "../utils.hpp"

constexpr int MAX_USERNAME_LEN = 20;

struct event_t {
    uint32_t len;
    uint32_t event_no;
    uint8_t event_type;

    const char* data;
    uint32_t data_len;
    uint32_t crc32;
    uint32_t total_len;
};

class ClientState {
    uint32_t game_id;
    const uint64_t session_id;
    std::string player_name;

    uint32_t next_expected_event_no;
    std::vector<std::string> players;

    uint8_t key;
    bool is_left_down, is_right_down;

    uint32_t width, height;

public:
    std::queue<std::string> events;

    explicit ClientState(std::string player_name_p, uint64_t session_id_p):
            game_id(0),
            session_id(session_id_p),
            player_name(std::move(player_name_p)),
            next_expected_event_no(0),
            key(0),
            is_left_down(false),
            is_right_down(false),
            width(0),
            height(0) {}

    static bool parse(const char *buffer, unsigned buff_len, struct event_t *res) {
        if (buff_len == 0) {
            return false;
        }

        res->len        = get_uint32(buffer);

        if (buff_len < 8 + res->len || res->len < 5) {
#ifdef DEBUG
            std::cout << "Invalid length of datagram, cannot calculate crc, ignoring" << std::endl;
#endif
            return false;
        }

        res->event_no   = get_uint32(buffer + 4);
        res->event_type = get_uint8(buffer + 8);
        res->data       = buffer + 9;

        res->data_len   = res->len - 5;
        res->total_len  = res->len + 8;

        res->crc32      = get_uint32(buffer + res->len + 4);

        if (crc32(buffer, res->len + 4) != res->crc32) {
#ifdef DEBUG
            std::cout << "Wrong control sum, ignoring" << std::endl;
#endif
            return false;
        }

        return true;
    }

    void parseMessage(const char *buffer, int len) {
        uint32_t game_id_rec = get_uint32(buffer);

        if (len < 4) return;

        if (game_id != game_id_rec) {
            struct event_t first_event{};

            if (parse(buffer + 4, len - 4, &first_event) && first_event.event_type == 0) {
                // Received event is a proper NEW_GAME event
                game_id = game_id_rec;
                next_expected_event_no = 0;
            }

            else {
#ifdef DEBUG
                std::cout << "Incorrect game_id, ignoring" << std::endl;
#endif
                return;
            }
        }

        buffer += 4;
        len -= 4;

        struct event_t event{};

        while (parse(buffer, len, &event)) {
            buffer += event.total_len;
            len -= event.total_len;
            parseEvent(&event);
        }
    }

    void parseEvent(const struct event_t *event) {

        if (next_expected_event_no != event->event_no) {
#ifdef DEBUG
            std::cout << "Wrong event number (expected"
                      << next_expected_event_no << ", got " << event->event_no
                      << "). Ignoring" << std::endl;
#endif
            return;
        }
        switch (event->event_type) {
            case 0:
                parseNewGame(event);
                break;
            case 1:
                parsePixel(event);
                break;

            case 2:
                parsePlayerEliminated(event);
                break;

            case 3:
                // GAME OVER
                break;

            default:;
#ifdef DEBUG
                std::cout << "Unrecognised type of event, stepping over" << std::endl;
#endif
        }
        next_expected_event_no++;
    }

    void parsePlayerEliminated(const struct event_t *event) {
        if (event->data_len != 1) {
            syserr("Incorrect size of PLAYER_ELIMINATED event, aborting.");
        }

        uint8_t player_number  = get_uint8(event->data);

        if (player_number >= players.size()) {
            syserr("Incorrect player number, aborting.");
        }

        events.push(
                "PLAYER_ELIMINATED " +
                players[player_number] + "\n");

    }

    void parsePixel(const struct event_t *event) {
        if (event->data_len != 9) {
            syserr("Incorrect size of PIXEL event, aborting.");
        }

        uint8_t player_number  = get_uint8(event->data);
        uint32_t x             = get_uint32(event->data + 1);
        uint32_t y             = get_uint32(event->data + 5);

        if (x >= width || y >= height) {
            syserr("Received illogical event: pixel out of the board, aborting.");
        }

        if (player_number >= players.size()) {
            syserr("Incorrect player number, aborting.");
        }

        events.push(
                "PIXEL " +
                std::to_string(x) + " " +
                std::to_string(y) + " " +
                players[player_number] + "\n");

    }

    void parseNewGame(const struct event_t *event) {
        if (event->data[event->data_len - 1] != '\0') {
            syserr("Incorrect event, player name is not null terminated, aborting");
        }

        width  = get_uint32(event->data);
        height = get_uint32(event->data + 4);

        std::string mess = "NEW_GAME " +
                std::to_string(width) + " " +
                std::to_string(height) + " ";

        std::string next_player_name;
        next_player_name.reserve(MAX_USERNAME_LEN);

        players.clear();

        for (const char *i = event->data + 8; i < event->data + event->data_len; ++i) {
            if (*i == '\0') {
                if (i != event->data + event->data_len - 1) {
                    mess += ' ';
                }

                if (next_player_name.empty() || next_player_name.size() > MAX_USERNAME_LEN) {
                    syserr("Invalid length of player name in NEW_GAME event, aborting.");
                }

                players.push_back(next_player_name);
                next_player_name.clear();
            }

            else if (*i < 33 || *i > 126) {
                syserr("Invalid character in NEW_GAME event, aborting.");
            }

            else {
                mess += *i;
                next_player_name += *i;
            }
        }
        events.push(mess + "\n");
    }

    /**
     * It parses a message received from GUI and modifies client's state accordingly.
     * @param buffer    a message as a sequence of characters (not null terminated),
     * @param buff_len  length of the message.
     */
    void parseGUI(const char *buffer, int buff_len) {
        char left_down[]    = "LEFT_KEY_DOWN\n";
        char left_up[]      = "LEFT_KEY_UP\n";
        char right_down[]   = "RIGHT_KEY_DOWN\n";
        char right_up[]     = "RIGHT_KEY_UP\n";

        if (std::strncmp(buffer, left_down, sizeof(left_down) - 1) == 0 && (sizeof(left_down) - 1) == buff_len) {
            is_left_down = true;
            key = 2;
        }
        else if (std::strncmp(buffer, left_up, sizeof(left_up) - 1) == 0 && (sizeof(left_up) - 1) == buff_len) {
            is_left_down = false;
            if (is_right_down) {
                key = 1;
            } else {
                key = 0;
            }
        }
        else if (std::strncmp(buffer, right_down, sizeof(right_down) - 1) == 0 && (sizeof(right_down) - 1) == buff_len) {
            is_right_down = true;
            key = 1;
        }
        else if (std::strncmp(buffer, right_up, sizeof(right_up) - 1) == 0 && (sizeof(right_up) - 1) == buff_len) {
            is_right_down = false;
            if (is_left_down) {
                key = 2;
            } else {
                key = 0;
            }
        }
        else {
            std::cout << "Not recognised message from gui, ignoring" << std::endl;
        }
    }

    /**
     * Generates content of a datagram that is send to server every 30ms.
     * @param mess      is an output parameter - a buffer which must be at least 34 bytes long,
     * @return          length generated message.
     */
    unsigned int generateServerMessage(char *mess) {
        put_uint64(mess, session_id);
        put_uint8(mess + 8, key);
        put_uint32(mess + 9, next_expected_event_no);
        strcpy(mess + 13, player_name.c_str());

        return player_name.size() + 13;
    }
};

#endif //CLIENT_STATE_HPP
