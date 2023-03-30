#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/times.h> /* 使用了times函数 */
#include <sys/types.h>
#include <sys/wait.h>

#define CHILD_RUN_TIME      10  /* 子进程的运行时间 */
#define CHILD_CPU_TIME      10
#define CHILD_IO_TIME       0
#define CHILD_PROCESS_NUM   30   /* 子进程数量 */

#define HZ	100 /* 每1/100秒产生1次时钟中断。*/

void cpuio_bound(int last, int cpu_time, int io_time);


int main(int argc, char * argv[])
{
    pid_t pid;
    int i = 0;
    
    while (i < CHILD_PROCESS_NUM) {
        if ((pid = fork()) < 0) {
            fprintf(stderr, "Error in fork()\n");
            return -1;
        } else if (pid == 0) { /* 以下为子进程执行 */
            /* 子进程执行指定时间后退出 */
            cpuio_bound(CHILD_RUN_TIME, CHILD_CPU_TIME - i, CHILD_IO_TIME + i);
            exit(0);
        } else { /* 以下在主进程中运行 */
            fprintf(stdout, "Process [%lu] created, parent [%lu].\n", (long)(pid), (long)(getpid()));
            ++i;
        }
    }
    
    while((pid = wait(NULL)) != -1) { /* 所有子进程退出后，父进程才退出；可在process.log中检验。*/
        fprintf(stdout, "Process [%lu] terminated, parent [%lu].\n", (long)(pid), (long)(getpid()));
    }
	
    return 0;
}

/*
 * 此函数按照参数占用CPU和I/O时间
 * last: 函数实际占用CPU和I/O的总时间，不含在就绪队列中的时间，>=0是必须的
 * cpu_time: 一次连续占用CPU的时间，>=0是必须的
 * io_time: 一次I/O消耗的时间，>=0是必须的
 * 如果last > cpu_time + io_time，则往复多次占用CPU和I/O
 * 所有时间的单位为秒
 */
void cpuio_bound(int last, int cpu_time, int io_time)
{
	struct tms start_time, current_time;
	clock_t utime, stime;
	int sleep_time;

	while (last > 0)
	{
		/* CPU Burst */
		times(&start_time);
		/* 其实只有t.tms_utime才是真正的CPU时间。但我们是在模拟一个
		 * 只在用户状态运行的CPU大户，就像“for(;;);”。所以把t.tms_stime
		 * 加上很合理。*/
		/* 为啥要算上核心态的CPU时间? */
		/* 用户CPU时间和系统CPU时间之和为CPU时间，即命令占用CPU执行的时间总和。*/
		do
		{
			times(&current_time);
			utime = current_time.tms_utime - start_time.tms_utime;
			stime = current_time.tms_stime - start_time.tms_stime;
		} while ( ( (utime + stime) / HZ )  < cpu_time );
		last -= cpu_time;

		if (last <= 0 )
			break;

		/* IO Burst */
		/* 用sleep(1)模拟1秒钟的I/O操作 */
		sleep_time=0;
		while (sleep_time < io_time)
		{
			sleep(1);
			sleep_time++;
		}
		last -= sleep_time;
	}
}

