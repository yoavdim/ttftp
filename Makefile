CC := gcc
CFLAGS ?= -std=c99 -Wall -Werror -pedantic-errors -DNDEBUG -D_GNU_SOURCE
OUTFILE := ttftps

SOURCES := $(wildcard *.c)
OBJECTS := $(patsubst %.c, %.o, $(SOURCES))

$(OUTFILE): $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(OUTFILE)
