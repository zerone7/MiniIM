TARGET = server
USER = user
STATUS = status
FRIEND = friend
MESSAGE = message

CC = gcc
INCDIR = ../include/
SRC = $(wildcard *.c)
HDRS = $(wildcard $(INCDIR)*.h)
LIBS = -lmysqlclient -lcrypto
DEFS = -D_MODULE_
CFLAGS = -I$(INCDIR)
VPATH = $(INCDIR) #vpath %.h ../include

MAINOBJ = $(SRC:.c=.o)
USEROBJ = user.o user_db.o modules.o
STATOBJ = status.o modules.o
FRDOBJ = friend.o friend_db.o modules.o
MSGOBJ = message.o message_db.o modules.o

.PHONY: all clean mclean
all: main

main: $(MAINOBJ)
	$(CC) -o $(TARGET) $(MAINOBJ) $(LIBS) 

$(MAINOBJ): $(HDRS)

modules: CFLAGS = -I$(INCDIR) $(DEFS) 
modules: $(USEROBJ) $(STATOBJ) $(FRDOBJ) $(MSGOBJ)
	$(CC) -o $(USER) $(USEROBJ) $(LIBS)
	$(CC) -o $(STATUS) $(STATOBJ) $(LIBS)
	$(CC) -o $(FRIEND) $(FRDOBJ) $(LIBS)
	$(CC) -o $(MESSAGE) $(MSGOBJ) $(LIBS)

clean:
	-rm *.o server status.data

mclean:
	-rm *.o $(USER) $(STATUS) $(FRIEND) $(MESSAGE) status.data
