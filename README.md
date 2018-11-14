# CS 3251 Project 2: TCP Hangman Game

## Description

A game-server and client-player implementation of a simple hangman game using TCP connections and coded in C.

## Installation

This application only uses the **C standard library**, so there's no need to install any additional packages/modules.

To compile, you need to have GCC installed. Then, open a terminal in a directory containing **server.c**, **client.c** and **MAKEFILE**, and use:

```
make
```

## Usage

To begin, start the server with:

```
./server [port number]
```

Then, clients should be able to connect with:

```
./client [server ip address] [port number]
```

Once connected, the client is asked whether they wish to play **single player** or **multiplayer** mode (currently unavailable). The server will, then, pick a random word and start the game. The client will be prompted to **guess a letter**. If it's contained in the word, its ocurrences will be revealed. If it's not, the letter will be added to an incorrect set. This keeps going until either the whole word is revealed or **6 total incorrect guesses** are made.

## Implementation

### Client

The client establishes a TCP connection to the server using the server and port arguments in its socket, and sends a packet with a 1-byte representation of the numbers 0 or 2, indicating game-start. In a loop, it, then, receives a packet from the server:

- If it's a **game control packet** ("flag"/first field is 0), the client displays some game information and waits for a valid input from the user. Once obtained, the input is sent to the server in the form: [0x01] [guess].

- If it's a **message packet** ("flag"/first field is the length of its contents), the client prints out it's contents and disconnects from the server (as it assumes that the packet is either a game-ending or server-overload message).

PACKET FORMAT: [msg length] [data]

### Server

The server starts using the port argument. Then, it creates a TCP socket and sets it to a passive listening state.

The server has a pre-established limit of 3 simultaneous games (enforced by the server overload message), and instantiates necessary variables for each of these. It keeps track of the connected clients using a constantly updated file descriptor list, as well as additional counters and arrays. It uses the function select() to detect requests and serve different clients as they come.

New connections send a **game-start packet**, and the server set up a new game accordingly. It answers with a game control packet, which contains all the game information needed by the client. When the game is finished, the server sends one more game control packet and, then, proceeds with the **final message packet** (which depends on the outcome of the game). Finally, the server closes the connection with the corresponding client, and resets all the used fields.

The server stays up, however, and continues to serve its current connections or accept new ones when possible.

PACKET FORMAT: [msg flag] [word length] [num incorrect] [data]

*The aforementioned **multiplayer mode** is, unfortunately, currently unavailable. The server implementation has the whole backing structure for accepting multiplayer games, including adapted game assignments and intermediate game states, but the game mechanics have not yet been implemented.*

## Credits

###Fernando Mello
[femello.me](http://femello.me)

femello@gatech.edu
