CC      = gcc
CFLAGS  = -Wall -Wextra -g -Iinclude
SRC     = $(wildcard src/*.c)
OBJ     = $(SRC:.c=.o)
EXAMPLES = $(wildcard examples/*.c)
BINS     = $(EXAMPLES:examples/%.c=build/%)

all: $(BINS)

build/%: examples/%.c $(OBJ) | build
	$(CC) $(CFLAGS) $< $(OBJ) -o $@

build:
	mkdir -p build

clean:
	rm -rf build src/*.o

.PHONY: all clean
