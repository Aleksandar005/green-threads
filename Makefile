CC      = gcc
CFLAGS  = -Wall -Wextra -g -Iinclude -D_XOPEN_SOURCE=600
SRC     = $(wildcard src/*.c)
OBJ     = $(SRC:.c=.o)
EXAMPLES = $(wildcard examples/*.c)
BINS     = $(EXAMPLES:examples/%.c=build/%)

UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
CFLAGS += -Wno-deprecated-declarations
endif

all: $(BINS)

build/%: examples/%.c $(OBJ) | build
	$(CC) $(CFLAGS) $< $(OBJ) -o $@

build:
	mkdir -p build

clean:
	rm -rf build src/*.o

.PHONY: all clean