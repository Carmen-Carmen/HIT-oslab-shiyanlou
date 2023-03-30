/*
 *  linux/lib/open.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>
#include <stdarg.h>

int open(const char * filename, int flag, ...)
{
	register int res;
	va_list arg;

	va_start(arg,flag);
	__asm__("int $0x80"                                     // 调用0x80中断
		:"=a" (res)                                     // 将eax中数据存入返回值res
		:"0" (__NR_open),"b" (filename),"c" (flag),     // eax传递系统调用号；ebx传递文件名指针；ecx传递flag
		"d" (va_arg(arg,int)));
	if (res>=0)
		return res;
	errno = -res;
	return -1;
}
