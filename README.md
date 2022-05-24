This is an implementation of a multiplayer game `Curve Fever`,
one of the Computer Networks course projects, at Faculty
of Mathematics, Mechanics and Informatics of the University of
Warsaw in academic year 2020/2021.

## Architecture

The game consists of three components: client, server and GUI. 
This repository contains implementations of server and client.
Server communicates with clients, manages the game state, receives
players' actions and sends them the changes in the game state.
Client communicates with server and GUI. 

## Run

Server can be run with
```
./screen-worms-server [-p n] [-s n] [-t n] [-v n] [-w n] [-h n]
```
* `-p n` – port number
* `-s n` – seed for random number generator
* `-t n` – turning speed
* `-v n` – game rounds per second
* `-w n` – width of playing area (default `640`)
* `-h n` – height of playing area (default `480`)

Client can be run with
```
./screen-worms-client game_server [-n player_name] [-p n] [-i gui_server] [-r n]
```
* `game_server` – IPv4 / IPv6 address or name of game server
* `-n player_name` – player name
* `-p n` – port of game server
* `-i gui_server` – IPv4 / IPv6 address or name of GUI server (default localhost)
* `-r n` – port of GUI server

## Protocol

### Client/server

Client communicates with server via UDP. A datagram sent by a client every 30ms contains
```
session_id: 8 bytes (unsigned)
turn_direction: 1 byte
next_expected_event_no: 4 bytes (unsigned)
player_name: 0-20 ASCII characters
```
To each such datagram, server responds with a datagram containing
```
game_id: 4 bytes (unsigned)
events: variable number of records
```
Such datagram is also sent to all connected players when a new game event occurs.

### Client/GUI

Client communicates with GUI server via TCP.
A GUI server receives messages in format
```
NEW_GAME maxx maxy player_name1 player_name2 ...
PIXEL x y player_name
PLAYER_ELIMINATED player_name
```
and sends messages `LEFT_KEY_DOWN`, `LEFT_KEY_UP`, `RIGHT_KEY_DOWN` or `RIGHT_KEY_UP`.
All components can communicate using IPv4 or IPv6.

### Events

A event is described by a record containing
```
len: 4 bytes -- sum of lengths of fields event_*
event_no: 4 bytes -- event number
event_type: 1 byte -- 0, 1, 2 or 3
event_data
crc32: 4 bytes -- control sum (CRC-32-IEEE)
```
An event can be one of the following types: `NEW_GAME` (0), `PIXEL` (1), `PLAYER_ELIMINATED` (2), `GAME_OVER` (3).
For each of them the correct format of `event_data` is defined, which I skip in this brief description.

### Game state & rounds

Server keeps the game state (`game_id`, position and direction of every player, playing area).
A new client can join anytime, if a round is in progress it becames an observer and receives a list of
all events of the game and can become a full player in the next round. Clients are identified with
socket and session_id. Datagrams from an unknown socket with a name of existing player are ignored.
A new game is started when the last one has ended and all players (at least two) pushed an arrow key. 

### Other

Delays in communicating with a subset of clients does not influence quality of communication with other clients.
Nagle's algorithm in client/GUI connection is disabled.