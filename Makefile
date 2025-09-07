CC = gcc
CFLAGS = -Wall -g

all: server client

server: server.o hashtable.o
	$(CC) $(CFLAGS) -o server server.o hashtable.o

client: client.o hashtable.o
	$(CC) $(CFLAGS) -o client client.o hashtable.o

server.o: server.c util.h hashtable.h
	$(CC) $(CFLAGS) -c server.c

client.o: client.c util.h hashtable.h
	$(CC) $(CFLAGS) -c client.c

hashtable.o: hashtable.c hashtable.h
	$(CC) $(CFLAGS) -c hashtable.c

clean:
	rm -f *.o server client
