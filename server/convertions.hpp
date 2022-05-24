/*
 * Author:   Witold Drzewakowski
 * Date:     2021-05-25
 * University of Warsaw
 */

#ifndef CONVERTIONS_HPP
#define CONVERTIONS_HPP

#include <sys/socket.h>

struct client_mess {
    uint64_t session_id;
    uint8_t turn_direction;
    uint32_t next_expected_event_no;
    std::string player_name;
    struct sockaddr_in6 *addr;
};

int is_client_mess_ok(int len) {
    return len >= 13 && len <= 33;
}

struct client_mess convert(char buff[], int len, struct sockaddr_in6 *addr) {
    struct client_mess res {
            get_uint64(buff),
            get_uint8(buff + 8),
            get_uint32(buff + 9),
            std::string(buff + 13, len - 13),
            addr
    };
    return res;
}


#endif //CONVERTIONS_HPP
