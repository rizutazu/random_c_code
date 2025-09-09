
main: m_thread.h m_thread.c main.c
	gcc -o main main.c m_thread.c -std=gnu11
main_debug: m_thread.h m_thread.c main.c
	gcc -g -o main_debug main.c m_thread.c -std=gnu11
main32: m_thread.h m_thread.c main.c
	gcc -m32 -o main32 main.c m_thread.c -std=gnu11
main32_debug: m_thread.h m_thread.c main.c
	gcc -g -m32 -o main32_debug main.c m_thread.c -std=gnu11
pingpong:  m_thread.h m_thread.c pingpong.c
	gcc -o pingpong pingpong.c m_thread.c -std=gnu11
produce_consume:  m_thread.h m_thread.c produce_consume.c
	gcc -o produce_consume produce_consume.c m_thread.c -std=gnu11
clean:
	rm -f main main_debug main32 main32_debug pingpong produce_consume

all: main main_debug main32 main32_debug produce_consume