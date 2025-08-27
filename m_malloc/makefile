CC := gcc

test_flags := -Dtest5 -Dtest1 -Dtest2 -Dtest3 -Dtest4 
debug_flag := -Dm_malloc_debug

flags := ${debug_flag} ${test_flags}

.PHONY: main clean test64 test32

main: 
	${CC} -o main m_malloc.c main.c ${flags}

clean:
	rm -f main test64 test32

test64: 
	${CC} -m64 -o test64 m_malloc.c tests.c ${flags}
	./test64

test32:
	${CC} -m32 -o test32 m_malloc.c tests.c ${flags}
	./test32