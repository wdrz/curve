/*
 * Author:   Witold Drzewakowski
 * Date:     2021-05-25
 * University of Warsaw
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/poll.h>

#include <arpa/inet.h>
#include <netinet/tcp.h>

#include <iostream>
#include <unistd.h>
#include <ctime>
#include <netdb.h>
#include <cstring>

#include "../utils.hpp"
#include "ClientState.hpp"

#define FREQ          30'000'000
#define BUFFER_SIZE   600
#define LINE_SIZE     100
#define GUI_MAX_MESS  300

void initServerUDPSocket(struct pollfd *p, const char *remote_port, const char *remote_name) {
    int sock;
    struct addrinfo addr_hints{}, *addr_result;

    std::memset(&addr_hints, 0, sizeof(struct addrinfo));
    addr_hints.ai_family = AF_UNSPEC; // IPv4 or v6
    addr_hints.ai_socktype = SOCK_DGRAM;
    addr_hints.ai_protocol = IPPROTO_UDP;

    if (getaddrinfo(remote_name, remote_port, &addr_hints, &addr_result) != 0) {
        syserr("getaddrinfo");
    }

    sock = socket(addr_result->ai_family, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) syserr("Failed to open UDP socket.");

    if (connect(sock, addr_result->ai_addr, addr_result->ai_addrlen) != 0) {
        syserr("connect on UDP socket (with server)");
    }

    freeaddrinfo(addr_result);

    p->fd = sock;
    p->revents = 0;
    p->events = POLLIN;
}



void initGUITCPSocket(struct pollfd *p, const char *remote_port, const char *remote_name) {
    int sock, rv, flag;
    struct addrinfo addr_hints{}, *addr_result;

    std::memset(&addr_hints, 0, sizeof(struct addrinfo));
    addr_hints.ai_family = AF_UNSPEC; // IPv4 or v6
    addr_hints.ai_socktype = SOCK_STREAM;
    addr_hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(remote_name, remote_port, &addr_hints, &addr_result) != 0) {
        syserr("getaddrinfo failed.");
    }

    sock = socket(addr_result->ai_family, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) syserr("Failed to open TCP socket.");

    flag = 1;
    rv = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (void *)&flag,sizeof(flag));
    if (rv < 0) syserr("setsockopt TCP_NODELAY failed.");

    if (connect(sock, addr_result->ai_addr, addr_result->ai_addrlen) != 0) {
        syserr("\"connect(...)\" on TCP socket with with gui server failed.");
    }

    freeaddrinfo(addr_result);

    p->fd = sock;
    p->revents = 0;
    p->events = POLLIN | POLLOUT;
}


void poll_routine(
        const char *port_server,
        const char *game_server,
        const char *port_gui,
        const char *gui_server,
        ClientState &cs) {

    char buffer[BUFFER_SIZE];
    ssize_t rcv_len, snd_len, len;

    struct pollfd p[3];
    std::memset(p, 0, sizeof(p));

    initServerUDPSocket(&p[1], port_server, game_server);
    initGUITCPSocket(&p[2], port_gui, gui_server);

    // Init timer
    struct itimerspec timerValue{};
    int timer_fd;

    timer_fd = timerfd_create(CLOCK_REALTIME, 0);
    if (timer_fd < 0)
        syserr("failed to create timer fd");

    bzero(&timerValue, sizeof(timerValue));
    timerValue.it_value.tv_sec = 0;
    timerValue.it_value.tv_nsec = FREQ;
    timerValue.it_interval.tv_sec = 0;
    timerValue.it_interval.tv_nsec = FREQ;

    // set events
    p[0].fd = timer_fd;
    p[0].revents = 0;
    p[0].events = POLLIN;

    // start timer
    if (timerfd_settime(timer_fd, 0, &timerValue, nullptr) < 0)
        syserr("could not start timer");

    // wait for events
    while (true) {
        p[0].revents = p[1].revents = p[2].revents = 0;

        int numEvents = poll(p, 3, -1);
        if (numEvents <= 0) {
            syserr("poll interrupted");
            break;
        }

        if (p[0].revents & (POLLIN | POLLERR)) {
            int64_t timersElapsed = 0;
            read(p[0].fd, &timersElapsed, 8);

            if (timersElapsed > 0) {
#ifdef DEBUG
                std::cout << "Writing message to server" << std::endl;
#endif
                len = cs.generateServerMessage(buffer);
                snd_len = write(p[1].fd, buffer, len);

                if (snd_len != len) {
                    std::cout << "UDP write unsuccessful" << std::endl;
                }
            }

        }

        if (p[1].revents & (POLLIN | POLLERR)) {
            rcv_len = read(p[1].fd, buffer, sizeof(buffer));
#ifdef DEBUG
            std::cout << "Message from server UDP, len = " << rcv_len << std::endl;
#endif
            if (rcv_len < 0)
                syserr("read");

            cs.parseMessage(buffer, rcv_len);
            if (!cs.events.empty()) {
               p[2].events = POLLOUT;
            }
        }

        if (p[2].revents & (POLLIN | POLLERR)) {
            rcv_len = read(p[2].fd, buffer, GUI_MAX_MESS);
#ifdef DEBUG
            std::cout << "Message from GUI: " << std::string(buffer, rcv_len) << std::endl;
#endif
            if (rcv_len < 0)
                syserr("recvfrom");

            if (rcv_len == 0) {
                syserr("GUI server disconnected");
            }

            cs.parseGUI(buffer, rcv_len);
        }

        if (p[2].revents & POLLOUT) {
            while (!cs.events.empty()) {
                len = cs.events.front().size();
                snd_len = write(p[2].fd, cs.events.front().c_str(), len);

                if (snd_len != len) {
#ifdef DEBUG
                    std::cout << "write unsuccessful" << std::endl;
#endif
                    break;
                }
                cs.events.pop();
            }
            p[2].events = POLLIN;
        }
    }

    if (close(p[1].fd) < 0)
        syserr("close");

    if (close(p[2].fd) < 0)
        syserr("close");
}


int main(int argc, char *argv[]) {
    uint64_t session_id      = time(nullptr);
    int c;

    if (argc < 2) {
        syserr("Usage: ./screen-worms-client game_server [-n player_name] [-p n] [-i gui_server] [-r n]");
    }

    std::string game_server  = argv[1];
    std::string player_name;
    std::string port_server  = "2021";
    std::string gui_server   = "localhost";
    std::string port_gui     = "20210";

    while ((c = getopt(argc - 1, argv + 1, "n:p:i:r:")) != -1)
        switch (c) {
            case 'n':
                player_name = (std::string) optarg;
                break;
            case 'p':
                if (parseNumericParam(optarg) < 0) {
                    syserr("Port number cannot be negative.");
                }
                port_server = (std::string) optarg;
                break;
            case 'i':
                gui_server = (std::string) optarg;
                break;
            case 'r':
                if (parseNumericParam(optarg) < 0) {
                    syserr("Port number cannot be negative.");
                }
                port_gui = (std::string) optarg;
                break;
            default:
                syserr("wrong argument");
        }

    ClientState cs{player_name, session_id};

    poll_routine(
            port_server.c_str(),
            game_server.c_str(),
            port_gui.c_str(),
            gui_server.c_str(),
            cs);

    return 0;
}