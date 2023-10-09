CC = gcc
LDFLAGS = -pthread -lrt

all: biboServer biboClient

biboServer: biboServer.c
	$(CC) -o biboServer biboServer.c $(LDFLAGS)

biboClient: biboClient.c
	$(CC) -o biboClient biboClient.c $(LDFLAGS)

clean:
	rm -f biboServer biboClient
