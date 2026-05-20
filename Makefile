CC = g++
CFLAGS = -Wall -g
LIBS = -lmysqlclient -levent -ljsoncpp -lpthread -lcrypto

all: server client stress_test

server: ser.cpp
	$(CC) $(CFLAGS) ser.cpp -o server $(LIBS)

client: client.cpp
	$(CC) $(CFLAGS) client.cpp -o client -ljsoncpp

stress_test: stress_test.cpp
	$(CC) $(CFLAGS) stress_test.cpp -o stress_test -ljsoncpp -lpthread

clean:
	rm -f server client stress_test
