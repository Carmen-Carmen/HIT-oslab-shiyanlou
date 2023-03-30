// 这里已经是内核要执行的程序了
#include <string.h> // 要调用strcpy
#include <errno.h>  // 要设定errno为EINVAL
#include <asm/segment.h>    // 要调用get_fs_byte和put_fs_byte, 实现在用户态和核心态内存空间的数据交换

#define maxSize 24          // 用户名不能超过24个字符
char __username[maxSize];   // 这个字符串实际上是一个全局变量
int __length;               // 记录用户名长度

int sys_iam(const char *name) { // name指针在who.c中指向用户态空间
    // printk("Hello from sys_iam\n");
    
    int name_len = 0;
    // calculate the len of *name
    while (get_fs_byte(&name[name_len]) != '\0') {  // 最后一个字符应为'\0'
        ++name_len;
    }
    // 如果超过23字符直接返回
    if (name_len > 23) {
        return -EINVAL;     // 因为_syscallXxx中会对出错的值取反，这里就先取反再返回，这样就能正确的返回
    }

    // 将传入的name的值获取并写入全局变量__username中（内核态）
    int i;
    for (i = 0; i < name_len; i++) {
        __username[i] = get_fs_byte(&name[i]);
        printk("username[%d]: %c\n", i, __username[i]);
    }
    __username[i] = '\0';   // 并且手动加上字符串结尾
    __length = name_len;    // 记录用户名长度
    return __length;
}

int sys_whoami(char *name, unsigned int size) {
    // printk("Hello from sys_whoami\n");

    // 如果传入的size < 所需空间（实际在内核态中__username的长度），就会对用户态的name越界访存，错误
    if (size < __length) {
        return -EINVAL;
    }

    int i;
    for (i = 0; i < __length; i++) {
        put_fs_byte(__username[i], &name[i]);       // 从内核态取出1个字符，写入用户态的name中
        printk("username[%d]: %c\n", i, __username[i]);
    }
    // 注意：不能使用printk打印用户空间的数据，否则会报segmentation fault

    return __length;
}
