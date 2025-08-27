extern void sys_write(unsigned int fd, const char *buf, unsigned int count);
extern void sys_exit(int code);

void print_hex(unsigned long long v) {
    static const char hex[] = {
        '0', '1', '2', '3', 
        '4', '5', '6', '7', 
        '8', '9', 'a', 'b', 
        'c', 'd', 'e', 'f'
    };
    char buf[16];
    for (int i = 0; i < 8; i++) {
        unsigned int byte = (unsigned int)(v >> (56 - i*8));
        // byte = byte & 0xff;
        buf[i*2] = hex[((byte & 0xf0) >> 4)];
        buf[i*2 + 1] = hex[byte & 0xf];
    }
    sys_write(1, "0x", 2);
    sys_write(1, buf, 16);
}

void my_main(unsigned long long *regs) {
    char *mapping[] = {
        "rax", "rbx", "rcx", "rdx", 
        "rdi", "rsi", "rbp", " r8", 
        " r9", "r10", "r11", "r12", 
        "r13", "r14", "r15", "rsp"
    };
    for (int i = 0; i < (sizeof(mapping) / sizeof(char *)); i++) {
        sys_write(1, mapping[i], 3);
        sys_write(1, ": ", 2);
        print_hex(regs[i]);
        sys_write(1, "\n", 1);
    }
    sys_exit(0);
}