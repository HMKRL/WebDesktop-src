OBJECT=server.o httpRequest.o SubBash.o session.o
LDLIBS=-pthread
CFLAGS=-g -Wall -std=c11
CC=gcc-6

all:$(OBJECT)
	$(CC) -o server $(OBJECT) $(CFLAGS) $(LDLIBS)
	cp ./server ../public/

%: %.c
	$(CC) -c $<

clean:
	rm -f *.o
	rm -f server
