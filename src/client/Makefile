CC = gcc
INCLUDE = -I../include
SRC = $(wildcard client*.c)
OBJS = $(SRC:.c=.o)
BIN = client
CFLAGS = -g -Wall

all: $(OBJS) $(BIN)

.PHONY: all clean

$(BIN): $(OBJS)
	$(CC) -o $(BIN) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $(INCLUDE) $< -o $@

clean:
	-rm *.o $(BIN)
