CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lserialport

SRC = src/main.c
OUT = music-kendama-bridge

.PHONY: all clean

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $(OUT) $(SRC) $(LDFLAGS)

clean:
	rm -f $(OUT)
