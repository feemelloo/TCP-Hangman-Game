/*///////////////////////////////////////////////////////////
*
* FILE:     server.c
* AUTHOR:   Fernando Mello
* PROJECT:  CS 3251 Project 2 - Professor Jun Xu 
* DESCRIPTION:  Hangman Game Server
*
*////////////////////////////////////////////////////////////

/* Included libraries */
#include <stdio.h>          /* printf() and fprintf() */
#include <sys/socket.h>     /* socket(), connect(), send(), and recv() */
#include <arpa/inet.h>      /* sockaddr_in and inet_addr() */
#include <stdlib.h>         /* all sorts of functionality */
#include <unistd.h>         /* close() */
#include <string.h>         /* any string ops */
#include <time.h>           /* time-keeping for withdrawal limit */

#define RCVBUFSIZE 2    /* Receive buffer size */
#define SNDBUFSIZE 22   /* Send buffer size */
#define MAXCLIENTS 6    /* Max number of simultaneously connected clients */
#define MAXGAMES 3      /* Max number of simultaneously running games */

/* The main function */
int main(int argc, char *argv[]) {
    int serverSock;                             /* Server socket */
    int clientSock[MAXCLIENTS] = {0};           /* Client sockets */
    int newSock;                                /* Temporary socket */
    int activeClients = 0;                      /* Number of connected clients */
    int newClient[MAXCLIENTS] = {0};            /* Whether client is newly connected */
    struct sockaddr_in servAddr;                /* Local address */
    struct sockaddr_in clntAddr;                /* Client address */
    unsigned short servPort;                    /* Server port */
    unsigned int clntLen = sizeof(clntAddr);    /* Length of address data struct */

    fd_set sockets;                             /* Set of socket descriptors */
    int sd;                                     /* Current socket descriptor */
    int maxSd;                                  /* Highest socket descriptor in sockets */

    char sndBuf[SNDBUFSIZE];                    /* Send buffer */
    char rcvBuf[RCVBUFSIZE];                    /* Receive buffer */
    char overBuf[SNDBUFSIZE];                   /* Server overload buffer */

    int gameMode[MAXGAMES];                     /* Game mode (1=single, 2=multiplayer) */
    int gameSelect[MAXCLIENTS];                 /* Game number assignments to each client */
    int playerID[MAXCLIENTS];                   /* Player identification (1 or 2) */
    int gameState[MAXGAMES] = {0};              /* Game state (0=open, 1=idle, 2=up, 3=over) */
    int activeGames = 0;                        /* Number of active games */

    char word[MAXGAMES][8];                     /* Game word */
    char wordShown[MAXGAMES][8];                /* Displayed word to player */
    char incorrect[MAXGAMES][6];                /* Set of incorrect guesses */
    int correctCheck[MAXGAMES];                 /* Check if guess is in game word */
    int correctCount[MAXGAMES];                 /* Number of correctly guessed letters */

    char* words[15];                            /* Set of all possible game words */
    words[0] = "georgia";
    words[1] = "tech";
    words[2] = "yellow";
    words[3] = "jackets";
    words[4] = "engineer";
    words[5] = "white";
    words[6] = "gold";
    words[7] = "burdell";
    words[8] = "sideways";
    words[9] = "wreck";
    words[10] = "swarm";
    words[11] = "buzz";
    words[12] = "rat";
    words[13] = "thwg";
    words[14] = "atlanta";

    char* loseMsg = "You Lose :(";              /* Message displayed after loss */
    char* winMsg = "You Win!";                  /* Message displayed after win */
    char* gameOverMsg = "Game Over!";           /* Message displayed after game end */
    char* overloadMsg = "server-overloaded";    /* Message displayed when MAXCLIENTS is reached */
    char* multiMsg = "Mode Unavailable";        /* Message displayed when multiplayer is chosen */

    int i, j;                                   /* Loop counters */
    int g;                                      /* Game selector */

    /* Check argument count */
    if (argc != 2) {
        printf("Incorrect number of arguments. The correct format is:\n\t"
               "./server serverPort\n");
        exit(1);
    }

    /* Get server port */
    servPort = atoi(argv[1]);

    /* Reserve space */
    memset(&rcvBuf, 0, MAXCLIENTS);
    memset(&sndBuf, 0, MAXCLIENTS);
    memset(&incorrect, 0, 6);

    for (i = 0; i < MAXGAMES; i++) {
        memset(&incorrect[i], 0, 6);
    }

    /* Prepare overload buffer */
    memset(overBuf, 17 & 0xFF, 1);
    memcpy(overBuf + 1, overloadMsg, 17);

    /* Set initial game assignments */
    for (i = 0; i < MAXCLIENTS; i++) {
        gameSelect[i] = MAXGAMES;
    }

    /* Create new TCP Socket for incoming requests */
    if ((serverSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        printf("socket() failed\n"); 
        exit(1);
    }

    /* Allow socket to be reused */
    if (setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &((int) {1}), sizeof(int)) < 0) {
        printf("setsockopt(SO_REUSEADDR) failed\n");
        exit(1);
    }

    /* Construct local address structure */
    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(servPort);
    
    /* Bind to local address structure */
    if (bind(serverSock, (struct sockaddr*) &servAddr, sizeof(servAddr)) < 0) {
        printf("bind() failed\n");
        exit(1);
    }
    
    /* Listen for incoming connections */
    if (listen(serverSock, 5) < 0) {
        printf("listen() failed\n");
        exit(1);
    }

    while (1) {
        /* Clear and add server socket to descriptor set */
        FD_ZERO(&sockets);
        FD_SET(serverSock, &sockets);
        maxSd = serverSock;
        
        for (i = 0; i < MAXCLIENTS; i++) {
            sd = clientSock[i];

            /* Add valid client sockets to descriptor set */
            if (sd > 0) {
                FD_SET(sd, &sockets);
            }

            /* Update max */
            if (sd > maxSd) {
                maxSd = sd;
            }
        }

        /* Select socket */
        if (select(maxSd + 1, &sockets, NULL, NULL, NULL) < 0) {
            printf("select() failed\n");
            exit(1);
        }

        /* New connection */
        if (FD_ISSET(serverSock, &sockets)) {
            /* Accept incoming connection */
            if ((newSock = accept(serverSock, (struct sockaddr*) &clntAddr, &clntLen)) < 0)  {
                printf("accept() failed\n");
                exit(1);
            }

            /* Check for server overload */
            if (activeGames == MAXGAMES) {
                if (send(newSock, overBuf, SNDBUFSIZE, 0) != SNDBUFSIZE) {
                    printf("send() failed\n");
                    exit(1);
                }

                close(newSock);
            }

            /* Add socket to socket list */
            for (i = 0; i < MAXCLIENTS && activeGames < MAXGAMES; i++) {
                if (clientSock[i] == 0) {
                    clientSock[i] = newSock;
                    newClient[i] = 0;
                    activeClients++;
                    break;
                }
            }
        }

        for (i = 0; i < MAXCLIENTS; i++) {
            sd = clientSock[i];
            g = gameSelect[i];

            /* Check if there's a pending request from each socket */
            if (FD_ISSET(sd, &sockets)) {
                if (newClient[i] == 0) {
                    /* Receive game start packet */
                    if (recv(sd, rcvBuf, RCVBUFSIZE, 0) < 0) {
                        printf("recv() failed\n");
                        exit(1);
                    }

                    if (rcvBuf[0] == 0x00) {    /* Single Player */
                        if (g == MAXGAMES) {
                            /* Select first open game */
                            for (j = 0; j < MAXGAMES; j++) {
                                if (gameState[j] == 0) {
                                    g = j;
                                    break;
                                }
                            }

                            /* Server overload */
                            if (g == MAXGAMES) {
                                if (send(sd, overBuf, SNDBUFSIZE, 0) != SNDBUFSIZE) {
                                    printf("send() failed\n");
                                    exit(1);
                                }

                                clientSock[i] = 0;
                                close(sd);
                                continue;
                            }
                        }

                        /* Set up game */
                        gameSelect[i] = g;
                        gameMode[g] = 1;
                        playerID[i] = 1;
                        gameState[g] = 2;
                    } else {                    /* Multiplayer */
                        /* Select first idle game */
                        for (j = 0; j < MAXGAMES; j++) {        /* Player 2 */
                            if (gameState[j] == 1) {
                                g = j;
                                playerID[i] = 2;
                                gameState[g] = 2;
                                break;
                            }
                        }

                        if (g == MAXGAMES) {
                            /* Select first open game */
                            for (j = 0; j < MAXGAMES; j++) {    /* Player 1 */
                                if (gameState[j] == 0) {
                                    g = j;
                                    playerID[i] = 1;
                                    gameState[g] = 1;
                                    break;
                                }
                            }
                        }

                        /* Server overload */
                        if (g == MAXGAMES) {
                            if (send(sd, overBuf, SNDBUFSIZE, 0) != SNDBUFSIZE) {
                                printf("send() failed\n");
                                exit(1);
                            }

                            clientSock[i] = 0;
                            close(sd);
                            continue;
                        }

                        /* Assign game to client */
                        gameSelect[i] = g;
                    }

                    /* Set connection to "old" */
                    newClient[i] = 1;

                    if (gameState[g] == 2) {
                        /* Select game word */
                        srand(time(NULL));
                        strcpy(word[g], words[rand() % 15]);

                        /* Build word displayed to player */
                        sprintf(wordShown[g], "___");

                        for (j = 3; j < strlen(word[g]); j++) {
                            sprintf(wordShown[g] + j, "_");
                        }

                        /* Reset correct guesses */
                        correctCount[g] = 0;

                        /* Increment game count */
                        activeGames++;

                        /* Build game control packet */
                        memset(sndBuf, 0x00, 1);
                        memset(sndBuf + 1, (char) (((int) strlen(word[g])) & 0xFF), 1);
                        memset(sndBuf + 2, (char) (((int) strlen(incorrect[g])) & 0xFF), 1);
                        memcpy(sndBuf + 3, wordShown[g], strlen(word[g]));
                        memcpy(sndBuf + 3 + (int) strlen(word[g]), incorrect[g], strlen(incorrect[g]));

                        /* Send game control packet */
                        if (send(sd, sndBuf, SNDBUFSIZE, 0) != SNDBUFSIZE) {
                            printf("send() failed\n");
                            exit(1);
                        }
                    } else {    /* Multiplayer unavailable */
                        memset(sndBuf, 16 & 0xFF, 1);
                        memcpy(sndBuf + 1, multiMsg, 16);

                        if (send(sd, sndBuf, SNDBUFSIZE, 0) != SNDBUFSIZE) {
                            printf("send() failed\n");
                            exit(1);
                        }

                        goto teardown;
                    }
                } else if (gameState[g] == 2) {             /* GAME RUNNING */
                    /* Receive guess */
                    if (recv(sd, rcvBuf, RCVBUFSIZE, 0) < 0) {
                        printf("recv() failed\n");
                        exit(1);
                    }

                    /* Check guess correctness */
                    correctCheck[g] = 0;
                 
                    for (j = 0; j < strlen(word[g]); j++) {
                        if (word[g][j] == rcvBuf[1]) {
                            correctCheck[g] = 1;
                            if (wordShown[g][j] == '_') {
                                correctCount[g]++;
                                sprintf(wordShown[g], "%.*s%c%.*s", j, wordShown[g], rcvBuf[1],
                                        (int) strlen(word[g]) - j - 1, wordShown[g] + j + 1);
                            }
                        }
                    }

                    /* Add guess to incorrect */
                    if (!correctCheck[g]) {
                        strcat(incorrect[g], &rcvBuf[1]);
                    }
                
                    /* Determine if game is over */
                    if (strlen(incorrect[g]) == 6 || correctCount[g] == strlen(word[g])) {
                        gameState[g] = 3;
                    }

                    /* Build game control packet */
                    memset(sndBuf, 0x00, 1);
                    memset(sndBuf + 1, (char) (((int) strlen(word[g])) & 0xFF), 1);
                    memset(sndBuf + 2, (char) (((int) strlen(incorrect[g])) & 0xFF), 1);
                    memcpy(sndBuf + 3, wordShown[g], strlen(word[g]));
                    memcpy(sndBuf + 3 + (int) strlen(word[g]), incorrect[g], strlen(incorrect[g]));

                    /* Send game control packet */
                    if (send(sd, sndBuf, SNDBUFSIZE, 0) != SNDBUFSIZE) {
                        printf("send() failed\n");
                        exit(1);
                    }
                } else if (gameState[g] == 3) {             /* GAME OVER */
                    if (strlen(incorrect[g]) == 6) {    /* Loss */
                        memset(sndBuf, 21 & 0xFF, 1);
                        memcpy(sndBuf + 1, loseMsg, 11);
                        memcpy(sndBuf + 12, gameOverMsg, 10);
                    } else {                            /* Win */
                        memset(sndBuf, 19 & 0xFF, 1);
                        memcpy(sndBuf + 1, winMsg, 9);
                        memcpy(sndBuf + 10, gameOverMsg, 10);
                    }

                    /* Send final message to client */
                    if (send(sd, sndBuf, SNDBUFSIZE, 0) != SNDBUFSIZE) {
                        printf("send() failed\n");
                        exit(1);
                    }

                    teardown:
                    /* Close socket and reset corresponding fields */
                    close(sd);
                    activeClients--;
                    activeGames--;
                    clientSock[i] = 0;
                    gameSelect[i] = MAXGAMES;
                    gameState[g] = 0;
                    correctCount[g] = 0;
                    memset(&rcvBuf, 0, RCVBUFSIZE);
                    memset(&sndBuf, 0, SNDBUFSIZE);
                    memset(&incorrect[g], 0, 6);
                    memset(&word[g], 0, 8);
                }
            }
        }
    }
}
