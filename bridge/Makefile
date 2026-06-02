CC ?= cc

# Resolve libserialport via pkg-config, else fall back to Homebrew's default prefix.
PKG := $(shell pkg-config --exists libserialport 2>/dev/null && echo yes)
ifeq ($(PKG),yes)
  SP_CFLAGS := $(shell pkg-config --cflags libserialport)
  SP_LIBS   := $(shell pkg-config --libs libserialport)
else
  SP_CFLAGS := -I/opt/homebrew/include
  SP_LIBS   := -L/opt/homebrew/lib -lserialport
endif

CFLAGS  = -Wall -Wextra -O2 -I../firmware/shared $(SP_CFLAGS)
LDFLAGS = $(SP_LIBS)

SRC = src/main.c
OUT = music-kendama-bridge

.PHONY: all clean

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $(OUT) $(SRC) $(LDFLAGS)

clean:
	rm -f $(OUT)
