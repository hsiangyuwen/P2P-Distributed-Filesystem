CC = gcc
CLIENT_OUT = Cli
CLIENT_SRC = Cli.c

SERVER_OUT = Ser
SERVER_SRC = Ser.c

NETIO_SRC = netio.c

all:$(CLIENT_SRC) $(SERVER_SRC)
	$(CC) -o $(CLIENT_OUT) $(CLIENT_SRC) $(NETIO_SRC) -lpthread
	$(CC) -o $(SERVER_OUT) $(SERVER_SRC) $(NETIO_SRC) -lpthread
