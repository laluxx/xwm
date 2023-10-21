CC = gcc

CFLAGS = -g -Wall

LDFLAGS = -lX11

SRCS = xwm.c

OUT = xwm

all: $(OUT)

$(OUT): $(SRCS) config.h
	$(CC) $(CFLAGS) $(SRCS) -o $(OUT) $(LDFLAGS)

clean:
	rm -f $(OUT)
