/*
 *  linux/kernel/fork.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *  'fork.c' contains the help-routines for the 'fork' system call
 * (see also system_call.s), and some misc functions ('verify_area').
 * Fork is rather simple, once you get the hang of it, but the memory
 * management can be a bitch. See 'mm/mm.c': 'copy_page_tables()'
 */
#include <errno.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <asm/system.h>

extern void write_verify(unsigned long address);

extern void first_return_from_kernel();

long last_pid=0;

void verify_area(void * addr,int size)
{
	unsigned long start;

	start = (unsigned long) addr;
	size += start & 0xfff;
	start &= 0xfffff000;
	start += get_base(current->ldt[2]);
	while (size>0) {
		size -= 4096;
		write_verify(start);
		start += 4096;
	}
}

int copy_mem(int nr,struct task_struct * p)
{
	unsigned long old_data_base,new_data_base,data_limit;
	unsigned long old_code_base,new_code_base,code_limit;

	code_limit=get_limit(0x0f);
	data_limit=get_limit(0x17);
	old_code_base = get_base(current->ldt[1]);
	old_data_base = get_base(current->ldt[2]);
	if (old_data_base != old_code_base)
		panic("We don't support separate I&D");
	if (data_limit < code_limit)
		panic("Bad data_limit");
	new_data_base = new_code_base = nr * 0x4000000;
	p->start_code = new_code_base;
	set_base(p->ldt[1],new_code_base);
	set_base(p->ldt[2],new_data_base);
	if (copy_page_tables(old_data_base,new_data_base,data_limit)) {
		printk("free_page_tables: from copy_mem\n");
		free_page_tables(new_data_base,data_limit);
		return -ENOMEM;
	}
	return 0;
}

/*
 *  Ok, this is the main fork-routine. It copies the system process
 * information (task[nr]) and sets up the necessary registers. It
 * also copies the data segment in it's entirety.
 */
int copy_process(int nr,long ebp,long edi,long esi,long gs,long none,
		long ebx,long ecx,long edx,
		long fs,long es,long ds,
		long eip,long cs,long eflags,long esp,long ss)
{
	struct task_struct *p;
	int i;
	struct file *f;

    long *krnstack; // 指向内核栈

	p = (struct task_struct *) get_free_page();
	if (!p) {
	    return -EAGAIN;
        }

        // 子进程内核栈的位置，准确说是内核栈基址上面一个地址
        krnstack = (long *) (PAGE_SIZE + (long)p);

        // 设置子进程内核栈
        *(--krnstack) = ss & 0xffff;    // --krnstack才是内核栈基址；注意long型指针加1是加了4个字节的！
        *(--krnstack) = esp;            // 先递减指向内核栈空位置
        *(--krnstack) = eflags;
        *(--krnstack) = cs & 0xffff;
        *(--krnstack) = eip;

        // 存入first_return_from_kernel中弹出的寄存器
        *(--krnstack) = ds & 0xffff;
        *(--krnstack) = es & 0xffff;
        *(--krnstack) = fs & 0xffff;
        *(--krnstack) = gs & 0xffff;
        *(--krnstack) = esi;
        *(--krnstack) = edi;
        *(--krnstack) = edx;

        *(--krnstack) = (long) first_return_from_kernel;    // 返回地址，first_return_from_kernel是汇编编号

        // switch_to中，完成切换内核栈后，在ret之前，会pop出4个寄存器，在此处先初始化
        *(--krnstack) = ebp;
        *(--krnstack) = ecx;
        *(--krnstack) = ebx;
        *(--krnstack) = 0;      // fork返回2个值，子进程返回0，存入eax中

	task[nr] = p;
	*p = *current;	/* NOTE! this doesn't copy the supervisor stack */

        p->kernelstack = krnstack;

	p->state = TASK_UNINTERRUPTIBLE;
	p->pid = last_pid;
	p->father = current->pid;
	p->counter = p->priority;
	p->signal = 0;
	p->alarm = 0;
	p->leader = 0;		/* process leadership doesn't inherit */
	p->utime = p->stime = 0;
	p->cutime = p->cstime = 0;
	p->start_time = jiffies;
	// 由于要记录新建时的滴答数，在设定滴答数之后就是新建进程的代码点
    fprintk(3, "%d\tN\t%ld\n", p->pid, jiffies);    // N 无->新建

// 注释掉使用tss进行切换的代码
// 	p->tss.back_link = 0;
// 	p->tss.esp0 = PAGE_SIZE + (long) p;
// 	p->tss.ss0 = 0x10;
// 	p->tss.eip = eip;
// 	p->tss.eflags = eflags;
// 	p->tss.eax = 0;
// 	p->tss.ecx = ecx;
// 	p->tss.edx = edx;
// 	p->tss.ebx = ebx;
// 	p->tss.esp = esp;
// 	p->tss.ebp = ebp;
// 	p->tss.esi = esi;
// 	p->tss.edi = edi;
// 	p->tss.es = es & 0xffff;
// 	p->tss.cs = cs & 0xffff;
// 	p->tss.ss = ss & 0xffff;
// 	p->tss.ds = ds & 0xffff;
// 	p->tss.fs = fs & 0xffff;
// 	p->tss.gs = gs & 0xffff;
// 	p->tss.ldt = _LDT(nr);
// 	p->tss.trace_bitmap = 0x80000000;
// 	if (last_task_used_math == current)
// 		__asm__("clts ; fnsave %0"::"m" (p->tss.i387));
                
	if (copy_mem(nr,p)) {
		task[nr] = NULL;
		free_page((long) p);
		return -EAGAIN;
	}
	for (i=0; i<NR_OPEN;i++)
		if ((f=p->filp[i]))
			f->f_count++;
	if (current->pwd)
		current->pwd->i_count++;
	if (current->root)
		current->root->i_count++;
	if (current->executable)
		current->executable->i_count++;
	set_tss_desc(gdt+(nr<<1)+FIRST_TSS_ENTRY,&(p->tss));
	set_ldt_desc(gdt+(nr<<1)+FIRST_LDT_ENTRY,&(p->ldt));
	p->state = TASK_RUNNING;	/* do this last, just in case */
	// 新建->就绪
    fprintk(3, "%d\tJ\t%ld\n", p->pid, jiffies);    // J 新建->就绪
	return last_pid;                // 父进程中的返回值
}

int find_empty_process(void)
{
	int i;

	repeat:
		if ((++last_pid)<0) last_pid=1;
		for(i=0 ; i<NR_TASKS ; i++)
			if (task[i] && task[i]->pid == last_pid) goto repeat;
	for(i=1 ; i<NR_TASKS ; i++)
		if (!task[i])
			return i;
	return -EAGAIN;
}
