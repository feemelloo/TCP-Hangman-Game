#################################################################
##
## FILE:	Makefile
## PROJECT:	CS 3251 Project 2 - Professor Jun Xu 
## DESCRIPTION: Compile Project 2
##
#################################################################

CC=gcc

OS := $(shell uname -s)

# Extra LDFLAGS if Solaris
ifeq ($(OS), SunOS)
	LDFLAGS=-lsocket -lnsl
    endif

all: client server 

client: client.c
	$(CC) client.c -o client

server: server.c
	$(CC) -l:libcrypto.so.10 server.c -o server

clean:
	    rm -f client server *.o