CC := gcc
CFLAGS := -lunwind -std=gnu11 -g
LIB := c_try_catch.c
HEADER := c_try_catch.h

.PHONY: main main32

main: main.c $(LIB) $(HEADER)
	$(CC) -o main main.c $(LIB) $(CFLAGS)
main32: main.c $(LIB) $(HEADER)
	$(CC) -o main32 main.c $(LIB) $(CFLAGS) -m32
recursive: recursive.c $(LIB) $(HEADER)
	$(CC) -o recursive recursive.c $(LIB) $(CFLAGS)
clean:
	rm -f main main32

all: main main32