CC := gcc

test_flags := -Dtest1 -Dtest2 -Dtest3 -Dtest4 -Dtest5
debug_flag := -Dm_malloc_debug

flags := ${debug_flag} ${test_flags}

.PHONY: main clean test

main: 
	${CC} -o main m_malloc.c main.c ${flags}

clean:
	rm -f main test

test: 
	${CC} -o test m_malloc.c tests.c ${flags}
	./test