PROGRAMS = screen-worms-client screen-worms-server
CC=g++
CPPFLAGS=-std=c++17 -Wall -Wextra -O2

all: $(PROGRAMS)

misc.o: server/misc.cpp server/misc.hpp
	$(CC) -c $(CPPFLAGS) -o $@ $<

server.o: server/main.cpp server/Board.hpp server/Client.hpp server/convertions.hpp server/Event.hpp server/Game.hpp server/misc.hpp server/Player.hpp utils.hpp
	$(CC) -c $(CPPFLAGS) -o $@ $<

client.o: client/main.cpp client/ClientState.hpp utils.hpp
	$(CC) -c $(CPPFLAGS) -o $@ $<

screen-worms-server: server.o misc.o
	$(CC) -o $@ $^

screen-worms-client: client.o
	$(CC) -o $@ $^

.PHONY: all clean

clean:
	rm -rf $(PROGRAMS) *.o
