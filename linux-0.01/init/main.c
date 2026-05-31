#define __LIBRARY__
#include <unistd.h>
#include <time.h>

/*
 * 我们需要把这些定义为内联 —— 在内核空间执行 fork 不会触发写时复制
 *（NO COPY ON WRITE !!!），直到执行 execve 为止。除了栈之外，这本身
 * 没有问题。处理办法是在 fork() 之后完全不让 main() 使用栈。因此，
 * 不能有任何函数调用 —— 这意味着 fork 也必须用内联代码，否则我们
 * 会在从 'fork()' 退出时使用栈。
 *
 * 实际上只有 pause 和 fork 需要内联，这样 main() 才不会对栈造成
 * 任何干扰，不过我们也顺便定义了其他几个。
 */
static inline _syscall0(int,fork)
static inline _syscall0(int,pause)
static inline _syscall0(int,setup)
static inline _syscall0(int,sync)

#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/head.h>
#include <asm/system.h>
#include <asm/io.h>

#include <stddef.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

#include <linux/fs.h>

static char printbuf[1024];

extern int vsprintf();
extern void init(void);
extern void hd_init(void);
extern long kernel_mktime(struct tm * tm);
extern long startup_time;

/*
 * 是的，是的，这很难看，但我找不到正确的做法，而这个似乎能用。
 * 如果有谁掌握更多关于实时时钟（real-time clock）的资料，我会很感兴趣。
 * 这里大部分内容都是反复试错得来的，外加读了一些 bios 的清单。唉。
 */

#define CMOS_READ(addr) ({ \
outb_p(0x80|addr,0x70); \
inb_p(0x71); \
})

#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)

static void time_init(void)
{
	struct tm time;

	do {
		time.tm_sec = CMOS_READ(0);
		time.tm_min = CMOS_READ(2);
		time.tm_hour = CMOS_READ(4);
		time.tm_mday = CMOS_READ(7);
		time.tm_mon = CMOS_READ(8)-1;
		time.tm_year = CMOS_READ(9);
	} while (time.tm_sec != CMOS_READ(0));
	BCD_TO_BIN(time.tm_sec);
	BCD_TO_BIN(time.tm_min);
	BCD_TO_BIN(time.tm_hour);
	BCD_TO_BIN(time.tm_mday);
	BCD_TO_BIN(time.tm_mon);
	BCD_TO_BIN(time.tm_year);
	startup_time = kernel_mktime(&time);
}

int main(void)		/* 它确实是 void，这里没有错误。 */
{			/* 启动例程（嗯，……）假定它是这样的 */
/*
 * 中断此时仍处于禁用状态。先完成必要的初始化设置，然后再开启中断。
 */
	time_init();
	tty_init();
	trap_init();
	sched_init();
	buffer_init();
	hd_init();
	sti();
	move_to_user_mode();
	if (!fork()) {		/* 我们指望这次能顺利成功 */
		init();
	}
/*
 *   注意！！   对于任何其他任务，'pause()' 都意味着我们必须收到一个
 * 信号才能被唤醒，但任务 0 是唯一的例外（参见 'schedule()'），因为
 * 任务 0 会在每一个空闲时刻（当没有其他任务可运行时）被激活。对任务 0
 * 来说，'pause()' 仅仅意味着我们去检查是否有其他任务可以运行，如果
 * 没有就返回到这里。
 */
	for(;;) pause();

	return 0;
}

static int printf(const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	write(1,printbuf,i=vsprintf(printbuf, fmt, args));
	va_end(args);
	return i;
}

static char * argv[] = { "/bin/sh",NULL };
static char * envp[] = { "HOME=/root","PATH=/bin","PWD=/", NULL };

void init(void)
{
	int i,j;

	setup();
	(void) open("/dev/tty0",O_RDWR,0);
	(void) dup(0);
	(void) dup(0);
	printf("%d buffers = %d bytes buffer space\n\r",NR_BUFFERS,
		NR_BUFFERS*BLOCK_SIZE);
	printf(" Ok.\n\r");
	if ((i=fork())<0)
		printf("Fork failed in init\r\n");
	else if (!i) {
		close(0);close(1);close(2);
		setsid();
		(void) open("/dev/tty0",O_RDWR,0);
		(void) dup(0);
		(void) dup(0);
		_exit(execve("/bin/sh",argv,envp));
	}
	j=wait(&i);
	printf("child %d died with code %04x\n",j,i);
	sync();
	_exit(0);	/* 注意！是 _exit，不是 exit() */
}
