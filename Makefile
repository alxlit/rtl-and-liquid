CC=gcc
CFLAGS=-I./include -Wall -g -O2
POST_CFLAGS=-lm -lc -lliquid -lpthread -lrtlsdr -lwebsockets
VPATH=./src

OBJS=app.o controller.o demod.o rtl.o scanner.o websocket.o

all: app

clean:
	rm -f *.o app

proposal:
	cd docs && latexmk -pdf proposal.tex

app: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(POST_CFLAGS) -o app
