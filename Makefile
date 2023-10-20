CC = gcc
CFLAGS = -Wall -Wextra
LIBS = -lX11 -g
OBJ = main.o tags.o

all: xwm

xwm: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o xwm

install: xwm
	sudo cp xwm /usr/bin/

uninstall:
	sudo rm -f /usr/bin/xwm

.PHONY: all clean install uninstall
