/*
 *  linux/kernel/system_call.s
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *  system_call.s  contains the system-call low-level handling routines.
 * This also contains the timer-interrupt handler, as some of the code is
 * the same. The hd- and flopppy-interrupts are also here.
 *
 * NOTE: This code handles signal-recognition, which happens every time
 * after a timer-interrupt and after each system call. Ordinary interrupts
 * don't handle signal-recognition, as that would clutter them up totally
 * unnecessarily.
 *
 * Stack layout in 'ret_from_system_call':
 *
 *	 0(%esp) - %eax
 *	 4(%esp) - %ebx
 *	 8(%esp) - %ecx
 *	 C(%esp) - %edx
 *	10(%esp) - %fs
 *	14(%esp) - %es
 *	18(%esp) - %ds
 *	1C(%esp) - %eip
 *	20(%esp) - %cs
 *	24(%esp) - %eflags
 *	28(%esp) - %oldesp
 *	2C(%esp) - %oldss
 */

SIG_CHLD	= 17

EAX		= 0x00
EBX		= 0x04
ECX		= 0x08
EDX		= 0x0C
FS		= 0x10
ES		= 0x14
DS		= 0x18
EIP		= 0x1C
CS		= 0x20
EFLAGS		= 0x24
OLDESP		= 0x28
OLDSS		= 0x2C

ESP0            = 4     # TSS中内核栈指针esp0放在偏移量为4的地方，见tss_struct的声明中，第一个long占据4个字节，esp0是第二个属性，故偏移量为4

# 这些是include/linux/sched.h中声明的task_struct中硬编码部分
state	= 0		# these are offsets into the task-struct.
counter	= 4
priority = 8
KERNEL_STACK = 12       # 新增一个long型的KERNEL_STACK，后面的偏移量都要+4
signal	= 16
sigaction = 20		# MUST be 16 (=len of sigaction)
blocked = (33*16 + 4)

# offsets within sigaction
sa_handler = 0
sa_mask = 4
sa_flags = 8
sa_restorer = 12

nr_system_calls = 72

/*
 * Ok, I get parallel printer interrupts while using the floppy for some
 * strange reason. Urgel. Now I just ignore them.
 */
.globl system_call,switch_to,first_return_from_kernel,sys_fork,timer_interrupt,sys_execve
.globl hd_interrupt,floppy_interrupt,parallel_interrupt
.globl device_not_available, coprocessor_error

# void switch_to(struct task_struct *pnext, unsigned long ldt)
.align 2    # 用于对齐的伪指令，必须从一个能被2整除的地址开始，为以下汇编函数的内存变量分配空间
switch_to: 
        push    %ebp            # 保存调用此函数的函数的栈帧基地址
        movl    %esp, %ebp      # 当前函数(switch_to)的栈帧基地址为栈顶地址
        pushl   %ecx
        pushl   %ebx
        pushl   %eax
        movl    8(%ebp), %ebx   # 调用switch_to的第一个参数，即pnext，它是指向目标进程的PCB，把它存进ebx寄存器中
        cmpl    %ebx, current   # 和current作比较，判断目标进程是否就是当前进程
        je      1f              # jmp指令的f (forward) 和 b (back)，表示向前还是向后跳；je 1f就是如果相等，就向后跳到汇编标号为1的地方执行

# current和pnext指向的进程不相同，需要进行进程切换
# 先切换PCB
        movl    %ebx, %eax      # 使eax指向目标进程PCB，即传入的pnext
        xchgl   %eax, current   # 用xchg long指令，快捷将eax和current所指内存的数据作交换，现在current指向目标进程PCB，而eax指向当前进程PCB（xchg指令不能使用立即数来作为操作数）
        
# TSS中的内核指针重写
        movl    tss, %ecx       # ecx指向当前进程的TSS
        addl    $4096, %ebx     # ebx指向目标进程PCB，+4096后指向目标进程的内核栈（？为啥）
        movl    %ebx, ESP0(%ecx)    # TSS的esp0指向目标进程的内核栈

# 切换内核栈栈顶指针（切换当前的内核栈为目标内核栈）；在这之前先保存被切换进程的esp
        movl    %esp, KERNEL_STACK(%eax)    # 把当前进程的esp保存到其PCB中；因为eax在之前操作中改为指向当前进程PCB
        movl    8(%ebp), %ebx   # 再一次将目标进程PCB存入ebx
        movl    KERNEL_STACK(%ebx), %esp    # 把目标进程PCB中的内核栈基址存储到esp寄存器

# 切换LDT
        movl    12(%ebp), %ecx  # 调用switch_to的第二个参数，即_LDT(next)，存入ecx中
        lldt    %cx             # load ldt指令，将cx寄存器的数据，即next进程的ldt存入LDTR寄存器；此时目标进程在执行用户态程序时使用的映射表就是自己的映射表了

# 切换LDT之后，更新fs，指向目标进程的用户态内存
        movl    $0x17, %ecx     # 用户空间数据段选择符为0x17
        mov     %cx, %fs

# 与clts配合处理协处理器
        cmpl    %eax, last_task_used_math
        jne     1f
        clts                    # 清除CR0寄存器中的任务切换flag (TS)
1:  # 恢复之前压栈保存的寄存器 
        popl    %eax
        popl    %ebx
        popl    %ecx
        popl    %ebp

# 利用ret完成PC的切换
        ret

# 在switch_to中，pop恢复寄存器值后，ret指令将esp指向的内容赋给eip，跳转到first_return_from_kernel中执行
# 填写子进程的内核栈
.align 2
first_return_from_kernel: 
        popl    %edx
        popl    %edi
        popl    %esi
        pop     %gs
        pop     %fs
        pop     %es
        pop     %ds
        iret

.align 2
bad_sys_call:
	movl $-1,%eax
	iret
.align 2
reschedule:
	pushl $ret_from_sys_call
	jmp schedule
.align 2
system_call:
	cmpl $nr_system_calls-1,%eax
	ja bad_sys_call
	push %ds
	push %es
	push %fs
	pushl %edx
	pushl %ecx		# push %ebx,%ecx,%edx as parameters
	pushl %ebx		# to the system call
	movl $0x10,%edx		# set up ds,es to kernel space
	mov %dx,%ds
	mov %dx,%es
	movl $0x17,%edx		# fs points to local data space
	mov %dx,%fs
	call sys_call_table(,%eax,4)
	pushl %eax
	movl current,%eax
	cmpl $0,state(%eax)		# state
	jne reschedule
	cmpl $0,counter(%eax)		# counter
	je reschedule
ret_from_sys_call:
	movl current,%eax		# task[0] cannot have signals
	cmpl task,%eax
	je 3f
	cmpw $0x0f,CS(%esp)		# was old code segment supervisor ?
	jne 3f
	cmpw $0x17,OLDSS(%esp)		# was stack segment = 0x17 ?
	jne 3f
	movl signal(%eax),%ebx
	movl blocked(%eax),%ecx
	notl %ecx
	andl %ebx,%ecx
	bsfl %ecx,%ecx
	je 3f
	btrl %ecx,%ebx
	movl %ebx,signal(%eax)
	incl %ecx
	pushl %ecx
	call do_signal
	popl %eax
3:	popl %eax
	popl %ebx
	popl %ecx
	popl %edx
	pop %fs
	pop %es
	pop %ds
	iret

.align 2
coprocessor_error:
	push %ds
	push %es
	push %fs
	pushl %edx
	pushl %ecx
	pushl %ebx
	pushl %eax
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax
	mov %ax,%fs
	pushl $ret_from_sys_call
	jmp math_error

.align 2
device_not_available:
	push %ds
	push %es
	push %fs
	pushl %edx
	pushl %ecx
	pushl %ebx
	pushl %eax
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax
	mov %ax,%fs
	pushl $ret_from_sys_call
	clts				# clear TS so that we can use math
	movl %cr0,%eax
	testl $0x4,%eax			# EM (math emulation bit)
	je math_state_restore
	pushl %ebp
	pushl %esi
	pushl %edi
	call math_emulate
	popl %edi
	popl %esi
	popl %ebp
	ret

.align 2
timer_interrupt:
	push %ds		# save ds,es and put kernel data space
	push %es		# into them. %fs is used by _system_call
	push %fs
	pushl %edx		# we save %eax,%ecx,%edx as gcc doesn't
	pushl %ecx		# save those across function calls. %ebx
	pushl %ebx		# is saved as we use that in ret_sys_call
	pushl %eax
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax
	mov %ax,%fs
	incl jiffies
	movb $0x20,%al		# EOI to interrupt controller #1
	outb %al,$0x20
	movl CS(%esp),%eax
	andl $3,%eax		# %eax is CPL (0 or 3, 0=supervisor)
	pushl %eax
	call do_timer		# 'do_timer(long CPL)' does everything from
	addl $4,%esp		# task switching to accounting ...
	jmp ret_from_sys_call

.align 2
sys_execve:
	lea EIP(%esp),%eax
	pushl %eax
	call do_execve
	addl $4,%esp
	ret

.align 2
sys_fork:
	call find_empty_process
	testl %eax,%eax
	js 1f
	push %gs
	pushl %esi
	pushl %edi
	pushl %ebp
	pushl %eax
	call copy_process
	addl $20,%esp
1:	ret

hd_interrupt:
	pushl %eax
	pushl %ecx
	pushl %edx
	push %ds
	push %es
	push %fs
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax
	mov %ax,%fs
	movb $0x20,%al
	outb %al,$0xA0		# EOI to interrupt controller #1
	jmp 1f			# give port chance to breathe
1:	jmp 1f
1:	xorl %edx,%edx
	xchgl do_hd,%edx
	testl %edx,%edx
	jne 1f
	movl $unexpected_hd_interrupt,%edx
1:	outb %al,$0x20
	call *%edx		# "interesting" way of handling intr.
	pop %fs
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	iret

floppy_interrupt:
	pushl %eax
	pushl %ecx
	pushl %edx
	push %ds
	push %es
	push %fs
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	movl $0x17,%eax
	mov %ax,%fs
	movb $0x20,%al
	outb %al,$0x20		# EOI to interrupt controller #1
	xorl %eax,%eax
	xchgl do_floppy,%eax
	testl %eax,%eax
	jne 1f
	movl $unexpected_floppy_interrupt,%eax
1:	call *%eax		# "interesting" way of handling intr.
	pop %fs
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	iret

parallel_interrupt:
	pushl %eax
	movb $0x20,%al
	outb %al,$0x20
	popl %eax
	iret
