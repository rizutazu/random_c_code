build: asm.s linker.ld my_main.c
	as --64 asm.s -o asm.o

	gcc -c -nostdlib -ffreestanding -fno-stack-protector -m64 my_main.c -o my_main.o

	ld -static -o my_main \
	-T linker.ld \
	asm.o my_main.o

clean:
	rm -f my_main asm.o my_main.o