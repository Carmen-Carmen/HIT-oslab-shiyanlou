#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/wait.h>

#define HZ  100

void cpuio_bound(int last, int cpu_time, int io_time);

int main(void) {
    pid_t pid;

    int count = 0;
	while (count < 5) {
        if ((pid = fork()) < 0) {
            fprintf(stderr, "Error in fork()\n");
            return -1;
        } else if (pid == 0) { /* 以下为子进程执行 */
            /* 子进程执行指定时间后退出 */
            cpuio_bound(10, 0, 5);
			exit(0);
        } else { /* 以下在主进程中运行 */
            fprintf(stdout, "Process [%lu] created, parent [%lu].\n", (long)(pid), (long)(getpid()));
            ++count;
        }
    }

    while ((pid = wait(NULL) != -1)) {
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

