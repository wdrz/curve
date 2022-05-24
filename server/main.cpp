/*
 * Author:   Witold Drzewakowski
 * Date:     2021-05-25
 * University of Warsaw
 */

#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <cstring>

#include <fcntl.h>
#include <csignal>

#include <iostream>
#include <future>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#include <sys/timerfd.h>
#include <sys/poll.h>

#include "../utils.hpp"
#include "misc.hpp"
#include "convertions.hpp"
#include "Game.hpp"

#define BUFFER_SIZE   600
#define LINE_SIZE     100
#define MAX_BOARD_DIM 4000
#define SECOND        1'000'000'000

void broadcastNewEvents(Game &game, int sock, char *buffer) {
    size_t len;
    socklen_t snd_addr_len;
    unsigned int from = game.board->event_to_broadcast;
    while (true) {

        len = game.buildDatagram(from, buffer);
        if (len <= 0) break;

        for (const auto &it: game.client_map) {
            snd_addr_len = sendto(sock, buffer, len, 0,
                    (struct sockaddr *) &it.second->addr, (socklen_t) sizeof(it.second->addr));

            if ((size_t) snd_addr_len != len) {
                std::cout << "Error (while \"broadcasting\" a new event) on sending data to client " << errno << " " << snd_addr_len << " " << len << std::endl;
                break;
            }
        }

    }
    game.board->event_to_broadcast = game.board->events.size();
}

void initUDPSocket(struct pollfd *p, const char *port) {
    int sock, rv;

    struct sockaddr_in6 server_address{};

    // Open socket
    sock = socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP); // creating IPv6 UDP socket
    if (sock < 0) syserr("socket");

    server_address.sin6_family = AF_INET6;
    server_address.sin6_addr = in6addr_any; // listening on all interfaces
    server_address.sin6_port = htons(atoi(port));


    int v6OnlyEnabled = 0;  // disable v-6 only mode
    if (setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &v6OnlyEnabled, sizeof(v6OnlyEnabled)) != 0)
        syserr("setsockopt");

    // bind the socket to a concrete address
    rv = bind(sock, (struct sockaddr*) &server_address,
              (socklen_t) sizeof(server_address));

    if (rv < 0) syserr("bind");

    if (fcntl(sock, F_SETFL, O_NONBLOCK) != 0) {
        syserr("fctl failed.");
    }

    p->fd = sock;
    p->revents = 0;
    p->events = POLLIN;
}


void server_routine(long freq, const char *port, Game &game) {
    ssize_t len;
    char buffer[BUFFER_SIZE], peer_addr[LINE_SIZE + 1];

    struct sockaddr_in6 client_address{};
    socklen_t snd_addr_len, rcv_addr_len;

    struct pollfd p[2];
    std::memset(p, 0, sizeof(p));

    int timer_fd;
    int64_t timers_elapsed;
    struct itimerspec timerValue{}, zeroValue{}, clearValue{};

    // set timerfd
    timer_fd = timerfd_create(CLOCK_REALTIME, 0);
    if (timer_fd < 0)
        syserr("failed to create timer fd");

    bzero(&timerValue, sizeof(timerValue));

    if (freq >= SECOND) {
        clearValue.it_value.tv_sec = 1;
        clearValue.it_value.tv_nsec = 0;
        clearValue.it_interval.tv_sec = 1;
        clearValue.it_interval.tv_nsec = 0;
    } else {
        clearValue.it_value.tv_sec = 0;
        clearValue.it_value.tv_nsec = freq;
        clearValue.it_interval.tv_sec = 0;
        clearValue.it_interval.tv_nsec = freq;
    }
    std::memcpy(&timerValue, &clearValue, sizeof(struct itimerspec));

    p[0].fd = timer_fd;
    p[0].revents = 0;
    p[0].events = POLLIN;

    initUDPSocket(&p[1], port);
    signal(SIGPIPE, SIG_IGN);

    // start timer
    if (timerfd_settime(timer_fd, 0, &timerValue, nullptr) < 0)
        syserr("could not start timer");

    // wait for events
    while (true) {
        p[0].revents = p[1].revents = 0;

        int rv = poll(p, 2, -1);

        if (rv <= 0) {
            std::cout << "poll interrupted" << std::endl;
            break;
        }

        if (p[0].revents & POLLIN) {
            timers_elapsed = 0;
            read(p[0].fd, &timers_elapsed, 8);
            game.disconnectInactiveClients();

            for (int64_t i = 0; i < timers_elapsed; i++) {
                if (game.isWaitingRoom()) {
                    break;
                }
                game.doRound();
            }

            broadcastNewEvents(game, p[1].fd, buffer);
        }

        if (p[1].revents & (POLLIN | POLLERR)) {
#ifdef DEBUG
            std::cout << "Receiving datagram from client socket" << std::endl;
#endif

            rcv_addr_len = (socklen_t) sizeof(client_address);
            len = recvfrom(p[1].fd, buffer, sizeof(buffer), 0,
                           (struct sockaddr *) &client_address, &rcv_addr_len);
            if (len <= 0) {
                std::cout << "error on datagram from client socket" << std::endl;
                continue;
            }

            if (is_client_mess_ok(len) != 1) {
#ifdef DEBUG
                std::cout << "Incorrect length of client message" << std::endl;
#endif
                continue;
            }

            // read address
            inet_ntop(AF_INET, &client_address.sin6_addr, peer_addr, LINE_SIZE);

            auto mess = convert(buffer, len, &client_address);
            std::string client_id = toSocket(peer_addr, ntohs(client_address.sin6_port));
#ifdef DEBUG
            std::cout
                    << "Session id: " << mess.session_id << std::endl
                    << "Turn direction: " << (unsigned) mess.turn_direction << std::endl
                    << "Next event: " << mess.next_expected_event_no << std::endl
                    << "Player name: " << mess.player_name << std::endl
                    << "Client address: " << peer_addr << std::endl
                    << "Client port: " << ntohs(client_address.sin6_port) << std::endl;
#endif
            if (!game.handleClient(client_id, mess)) {
                // datagram contains somehow invalid data, must be ignored
#ifdef DEBUG
                std::cout << "Datagram logically invalid, ignoring" << std::endl;
#endif
                continue;
            }

            if (game.isWaitingRoom()) {
                if (game.waitingRoomRoutine(client_id)) {
                    // game has been started, reset timer

                    bzero(&zeroValue, sizeof(zeroValue));
                    if (timerfd_settime(timer_fd, 0, &zeroValue, nullptr) == -1) {
                        syserr("timerfd_settime");
                    }

                    std::memcpy(&timerValue, &clearValue, sizeof(struct itimerspec));

                    if (timerfd_settime(timer_fd, 0, &timerValue, nullptr) < 0)
                        syserr("timerfd_settime");

                }
            }

            while (true) {
                len = game.buildDatagram(mess.next_expected_event_no, buffer);
                if (len <= 0) break;

                snd_addr_len = sendto(p[1].fd, buffer, len, 0,
                                      (struct sockaddr *) &client_address, (socklen_t) sizeof(client_address));

                if (snd_addr_len != len) {
#ifdef DEBUG
                    std::cout << "Error on sending data to client " << errno << " " << snd_addr_len << " " << len << std::endl;
#endif
                    break;
                }
            }
        }

    }

    if (close(p[1].fd) < 0)
        syserr("close");
}

int main(int argc, char *argv[]) {
    std::string port    = "2021";

    uint32_t seed            = time(nullptr);
    int turning_speed        = 6;
    long rounds_per_sec      = 50;
    int width                = 640;
    int height               = 480;

    int c;

    while ((c = getopt(argc, argv, "p:s:t:v:w:h:")) != -1)
        switch (c) {
            case 'p':
                if (parseNumericParam(optarg) < 0) {
                    syserr("Port number cannot be negative.");
                }
                port = (std::string) optarg;
                break;
            case 's':
                seed = parseUnsignedNumericParam(optarg);
                break;
            case 't':
                turning_speed = parseNumericParam(optarg);
                break;
            case 'v':
                rounds_per_sec = parseNumericParam(optarg);
                break;
            case 'w':
                width = parseNumericParam(optarg);
                break;
            case 'h':
                height = parseNumericParam(optarg);
                break;
            default:
                syserr("Usage: ./screen-worms-server [-p n] [-s n] [-t n] [-v n] [-w n] [-h n]");
        }

    if (width <= 0 || width > MAX_BOARD_DIM || height <= 0 || height > MAX_BOARD_DIM) {
        syserr("Provided board size is unreasonable (width and height should be between 1 and 50000).");
    }

    if (turning_speed > 90 || turning_speed < -90 || turning_speed == 0) {
        syserr("Provided turning_speed is unreasonable (should be between -90 and 90) and not equal 0.");
    }

    if (rounds_per_sec <= 0 || rounds_per_sec > 500) {
        syserr("Provided number of rounds per second is unreasonable (should be between 1 and 500).");
    }

    if (optind < argc) {
        syserr("Non-option argument. Usage: ./screen-worms-server [-p n] [-s n] [-t n] [-v n] [-w n] [-h n]");
    }

    Random::set(seed);
    Game game{turning_speed, width, height};

    std::cout << "Listening on port: " << port << std::endl;

    server_routine(SECOND /  rounds_per_sec, port.c_str(), game);

    return 0;
}