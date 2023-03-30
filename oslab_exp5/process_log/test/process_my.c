#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/wait.h>

#define HZ  100

void cpuio_bound(int last, int cpu_time, int io_time);

int ppid;

int main(void) {
    int pid;

    ppid = getpid();    /* record the id of parent process */ 

    if ((pid = fork()) < 0) {   /* the return of fork below 0, refers to error */ 
        fprintf(stderr, "Error in fork()!\n");
    /* after fork, the parent process is copied to a child process; */ 
    /* and the 2 processes will execute the code below together */ 
    } else if (pid == 0) {       /* the return of fork is 0, which is what fork returns in a child process */ 
        /* the code block will be executed in the child process; */ 
        cpuio_bound(10, 9, 1);
    } else {                    /* pid > 0, which is the pid of the newly created child process */ 
        /* the code block will be executed in the parent process; */ 
        /* notice that in here pid refers to the created child process */ 
        /* and function getpid() returns the pid of current process (which is the parent process) */ 
        fprintf(stdout, "Process [%lu] created by process [%lu].\n", (long)(pid), (long)(getpid()));
    }
    
    if (getpid() != ppid) {
        goto parent;
    }

    if ((pid = fork()) < 0) {   
        fprintf(stderr, "Error in fork()!\n");
    } else if (pid == 0) {       
        cpuio_bound(10, 1, 9);
    } else {
        fprintf(stdout, "Process [%lu] created by process [%lu].\n", (long)(pid), (long)(getpid()));
    }

    if (getpid() != ppid) {
        goto parent;
    }

    if ((pid = fork()) < 0) {   
        fprintf(stderr, "Error in fork()!\n");
    } else if (pid == 0) {       
        cpuio_bound(10, 0, 1);
    } else {
        fprintf(stdout, "Process [%lu] created by process [%lu].\n", (long)(pid), (long)(getpid()));
    }

    if (getpid() != ppid) {
        goto parent;
    }

    if ((pid = fork()) < 0) {   
        fprintf(stderr, "Error in fork()!\n");
    } else if (pid == 0) {       
        cpuio_bound(10, 1, 0);
    } else {
        fprintf(stdout, "Process [%lu] created by process [%lu].\n", (long)(pid), (long)(getpid()));
    }

parent: 
    while ((pid = wait(NULL)) != -1) {
    /* function wait will make the current process to wait */ 
    /* for a child process to exit */ 
    /*      - when a child process exits, wait(NULL) returns its pid */
    /*      - when there is no child process, wait(NULL) returns -1 */ 
    /* therefore, the "wait(NULL) != -1" here mean to wait until all the child process exit */
        fprintf(stdout, "Process [%lu] terminated.\n", (long)(pid));
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

