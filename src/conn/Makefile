CC = gcc
INCLUDE = -I../include
SRC = $(wildcard *.c)
OBJS = $(SRC:.c=.o)
BIN = conn_server

all: $(OBJS) $(BIN)

.PHONY: all clean

$(BIN): $(OBJS)
	$(CC) -o $(BIN) $(OBJS)

%.o: %.c
	$(CC) -c $(INCLUDE) $< -o $@

clean:
	-rm *.o $(BIN)
