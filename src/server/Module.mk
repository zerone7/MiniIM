Target = server
CC = gcc
CFLAGS = -I../include -D _MODULE_
vpath %.h ../include

mainobj = main.o message.o user.o friend.o status.o modules.o user_db.o message_db.o friend_db.o

.PHONY: all
all: main

$(mainobj):	message.h user.h friend.h status.h protocol.h modules.h user_db.h message_db.h friend_db.h

main: $(mainobj)
	$(CC) -o user user.o user_db.o modules.o -lmysqlclient
	$(CC) -o status status.o modules.o -lmysqlclient
	$(CC) -o friend friend.o friend_db.o modules.o -lmysqlclient
	$(CC) -o message message.o message_db.o modules.o -lmysqlclient

.PHONY: clean
clean:
	-rm *.o user status friend message
