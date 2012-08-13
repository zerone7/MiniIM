Target = server
CC = gcc
CFLAGS = -I../include 
vpath %.h ../include

mainobj = main.o message.o user.o friend.o status.o modules.o user_db.o message_db.o

.PHONY: all
all: main

$(mainobj):	message.h user.h friend.h status.h protocol.h modules.h user_db.h message_db.h

main: $(mainobj)
	$(CC) -o $(Target) $(mainobj) -lmysqlclient

.PHONY: clean
clean:
	-rm *.o server
