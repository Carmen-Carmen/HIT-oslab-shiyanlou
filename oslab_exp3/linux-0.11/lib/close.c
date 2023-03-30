/*
 *  linux/lib/close.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>

_syscall1(int,close,int,fd)

// 经过_syscall1宏展开后
// int close(int fd) {
//     long __res;
//     __asm__ volatile ("int $0x80"                // 中断调用
//         : "=a" (__res)i                          // 调用返回后，将从EAX取出返回值，存入__res
//         : "0" (__NR_close), "b" ((long) (fd)));  // 将宏 __NR_close存入EAX，将参数fd（文件描述符）存入EBX
// 
//     if (__res >= 0) {
//         return (int) __res;
//     }
// 
//     errno = -__res;
//     return -1;
// }
