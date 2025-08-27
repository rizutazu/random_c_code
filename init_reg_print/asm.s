.intel_syntax noprefix
.global _start
_start:
    push rsp
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rsi
    push rdi
    push rdx
    push rcx
    push rbx
    push rax
    mov rdi, rsp
    call my_main
    hlt

.global sys_exit
sys_exit:
    mov rax, 60
    syscall

.global sys_write
sys_write:
    mov rax, 1
    syscall
    ret
