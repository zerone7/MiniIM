CC = gcc
INCLUDE = -I../include
SRC = $(wildcard *.c)
OBJS = $(SRC:.c=.o)
BIN = conn_server
CFLAGS = -g -Wall

all: $(OBJS) $(BIN)

.PHONY: all clean

$(BIN): $(OBJS)
	$(CC) -o $(BIN) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $(INCLUDE) $< -o $@

clean:
	-rm *.o $(BIN)
