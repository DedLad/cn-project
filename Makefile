CC=gcc
CFLAGS=-Wall -O2

OS ?= $(shell uname -s)
ifeq ($(OS),Windows_NT)
    TARGETS=server.exe client.exe
    PLATFORM=platform/win_screen.c
    LIBS=-lgdi32 -lws2_32
else ifeq ($(OS),Linux)
    TARGETS=server client
    PLATFORM=platform/linux_screen.c
    LIBS=-lX11
else
    $(error Unsupported OS: $(OS))
endif

all: $(TARGETS)

server.exe: server.c $(PLATFORM) common.h
	$(CC) $(CFLAGS) -o $@ server.c $(PLATFORM) $(LIBS)

client.exe: client.c common.h
	$(CC) $(CFLAGS) -o $@ client.c $(LIBS)

server: server.c $(PLATFORM) common.h
	$(CC) $(CFLAGS) -o $@ server.c $(PLATFORM) $(LIBS)

client: client.c common.h
	$(CC) $(CFLAGS) -o $@ client.c $(LIBS)

clean:
	rm -f server client server.exe client.exe
