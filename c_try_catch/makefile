CC := gcc
OPTIMIZATION_LEVEL := 0
CFLAGS := -std=gnu11 -O$(OPTIMIZATION_LEVEL) -fno-omit-frame-pointer
DEBUG_FLAGS := -g -Dc_try_catch_debug
LIB := c_try_catch.c
HEADER := c_try_catch.h

.PHONY: main main_debug main32 recursive uncaught_exit

main:
	$(CC) -o main main.c $(LIB) $(CFLAGS)
main_debug:
	$(CC) -o main_debug main.c $(LIB) $(CFLAGS) $(DEBUG_FLAGS)
main32:
	$(CC) -o main32 main.c $(LIB) $(CFLAGS) -m32
recursive:
	$(CC) -o recursive recursive.c $(LIB) $(CFLAGS)
uncaught_exit:
	$(CC) -o uncaught_exit uncaught_exit.c $(LIB) $(CFLAGS) $(DEBUG_FLAGS)
clean:
	rm -f main main32 main_debug recursive uncaught_exit