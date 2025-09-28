CC := gcc
CFLAGS := -lunwind -std=gnu11 -g
LIB := c_try_catch.c
HEADER := c_try_catch.h

.PHONY: main main32

main: main.c $(LIB) $(HEADER)
	$(CC) -o main main.c $(LIB) $(HEADER) $(CFLAGS)
main32: main.c $(LIB) $(HEADER)
	$(CC) -o main32 main.c $(LIB) $(HEADER) $(CFLAGS) -m32
clean:
	rm -f main main32

all: main main32