#ifndef _SCHED_H
#define _SCHED_H

#define NR_TASKS 64
#define HZ 100

#define FIRST_TASK task[0]
#define LAST_TASK task[NR_TASKS-1]

#include <linux/head.h>
#include <linux/fs.h>
#include <linux/mm.h>

#if (NR_OPEN > 32)
#error "Currently the close-on-exec-flags are in one word, max 32 files/proc"
#endif

#define TASK_RUNNING		0
#define TASK_INTERRUPTIBLE	1
#define TASK_UNINTERRUPTIBLE	2
#define TASK_ZOMBIE		3
#define TASK_STOPPED		4

#ifndef NULL
#define NULL ((void *) 0)
#endif

extern int copy_page_tables(unsigned long from, unsigned long to, long size);
extern int free_page_tables(unsigned long from, long size);

extern void sched_init(void);
extern void schedule(void);
extern void trap_init(void);
extern void panic(const char * str);
extern int tty_write(unsigned minor,char * buf,int count);

typedef int (*fn_ptr)();

struct i387_struct {
	long	cwd;
	long	swd;
	long	twd;
	long	fip;
	long	fcs;
	long	foo;
	long	fos;
	long	st_space[20];	/* 8 个浮点寄存器，每个 10 字节 = 80 字节 */
};

struct tss_struct {
	long	back_link;	/* 高 16 位为 0 */
	long	esp0;
	long	ss0;		/* 高 16 位为 0 */
	long	esp1;
	long	ss1;		/* 高 16 位为 0 */
	long	esp2;
	long	ss2;		/* 高 16 位为 0 */
	long	cr3;
	long	eip;
	long	eflags;
	long	eax,ecx,edx,ebx;
	long	esp;
	long	ebp;
	long	esi;
	long	edi;
	long	es;		/* 高 16 位为 0 */
	long	cs;		/* 高 16 位为 0 */
	long	ss;		/* 高 16 位为 0 */
	long	ds;		/* 高 16 位为 0 */
	long	fs;		/* 高 16 位为 0 */
	long	gs;		/* 高 16 位为 0 */
	long	ldt;		/* 高 16 位为 0 */
	long	trace_bitmap;	/* 位定义：跟踪位为位 0，位图为位 16-31 */
	struct i387_struct i387;
};

struct task_struct {
/* 这些是硬编码的 - 不要改动 */
	long state;	/* -1 不可运行，0 可运行，>0 已停止 */
	long counter;
	long priority;
	long signal;
	fn_ptr sig_restorer;
	fn_ptr sig_fn[32];
/* 各种字段 */
	int exit_code;
	unsigned long end_code,end_data,brk,start_stack;
	long pid,father,pgrp,session,leader;
	unsigned short uid,euid,suid;
	unsigned short gid,egid,sgid;
	long alarm;
	long utime,stime,cutime,cstime,start_time;
	unsigned short used_math;
/* 文件系统信息 */
	int tty;		/* 没有 tty 时为 -1，所以必须是有符号数 */
	unsigned short umask;
	struct m_inode * pwd;
	struct m_inode * root;
	unsigned long close_on_exec;
	struct file * filp[NR_OPEN];
/* 本任务的 ldt：0 - 空描述符，1 - cs，2 - ds&ss */
	struct desc_struct ldt[3];
/* 本任务的 tss */
	struct tss_struct tss;
};

/*
 * INIT_TASK 用于设置第一个任务表，改动它后果自负！
 * 基址 Base=0，段限长 limit=0x9ffff（=640kB）
 */
#define INIT_TASK \
/* 状态等 */	{ 0,15,15, \
/* 信号 */	0,NULL,{(fn_ptr) 0,}, \
/* ec,brk 等 */	0,0,0,0,0, \
/* pid 等 */	0,-1,0,0,0, \
/* uid 等 */	0,0,0,0,0,0, \
/* 报警定时 */	0,0,0,0,0,0, \
/* 数学协处理器 */	0, \
/* 文件系统信息 */	-1,0133,NULL,NULL,0, \
/* filp */	{NULL,}, \
	{ \
		{0,0}, \
/* ldt */	{0x9f,0xc0fa00}, \
		{0x9f,0xc0f200}, \
	}, \
/*tss*/	{0,PAGE_SIZE+(long)&init_task,0x10,0,0,0,0,(long)&pg_dir,\
	 0,0,0,0,0,0,0,0, \
	 0,0,0x17,0x17,0x17,0x17,0x17,0x17, \
	 _LDT(0),0x80000000, \
		{} \
	}, \
}

extern struct task_struct *task[NR_TASKS];
extern struct task_struct *last_task_used_math;
extern struct task_struct *current;
extern long volatile jiffies;
extern long startup_time;

#define CURRENT_TIME (startup_time+jiffies/HZ)

extern void sleep_on(struct task_struct ** p);
extern void interruptible_sleep_on(struct task_struct ** p);
extern void wake_up(struct task_struct ** p);

/*
 * 第一个 TSS 在 gdt 中的入口位置。0-空描述符，1-cs，2-ds，3-系统调用
 * 4-TSS0，5-LDT0，6-TSS1 等等 ...
 */
#define FIRST_TSS_ENTRY 4
#define FIRST_LDT_ENTRY (FIRST_TSS_ENTRY+1)
#define _TSS(n) ((((unsigned long) n)<<4)+(FIRST_TSS_ENTRY<<3))
#define _LDT(n) ((((unsigned long) n)<<4)+(FIRST_LDT_ENTRY<<3))
#define ltr(n) __asm__("ltr %%ax"::"a" (_TSS(n)))
#define lldt(n) __asm__("lldt %%ax"::"a" (_LDT(n)))
#define str(n) \
__asm__("str %%ax\n\t" \
	"subl %2,%%eax\n\t" \
	"shrl $4,%%eax" \
	:"=a" (n) \
	:"a" (0),"i" (FIRST_TSS_ENTRY<<3))
/*
 *	switch_to(n) 把任务切换到任务号 n，首先检查 n 是否就是
 * 当前任务，如果是则什么也不做。如果切换到的任务是最近一个
 * 使用过数学协处理器的任务，还会清除 TS 标志。
 */
#define switch_to(n) {\
struct {long a,b;} __tmp; \
__asm__ __volatile__("cmpl %%ecx,current\n\t" \
	"je 1f\n\t" \
	"xchgl %%ecx,current\n\t" \
	"movw %%dx,%1\n\t" \
	"ljmp *%0\n\t" \
	"cmpl %%ecx,%2\n\t" \
	"jne 1f\n\t" \
	"clts\n" \
	"1:\n\t" \
	::"m" (*&__tmp.a),"m" (*&__tmp.b), \
	"m" (last_task_used_math),"d" _TSS(n),"c" ((long) task[n])); \
}

#define PAGE_ALIGN(n) (((n)+0xfff)&0xfffff000)

#define _set_base(addr,base) do { unsigned long __pr; \
__asm__ __volatile__ ("movw %%dx,%1\n\t" \
	"rorl $16,%%edx\n\t" \
	"movb %%dl,%2\n\t" \
	"movb %%dh,%3" \
	:"=&d" (__pr) \
	:"m" (*((addr)+2)), \
	 "m" (*((addr)+4)), \
	 "m" (*((addr)+7)), \
         "0" (base) \
        ); } while(0)

#define _set_limit(addr,limit) do { unsigned long __lr; \
__asm__ __volatile__ ("movw %%dx,%1\n\t" \
	"rorl $16,%%edx\n\t" \
	"movb %2,%%dh\n\t" \
	"andb $0xf0,%%dh\n\t" \
	"orb %%dh,%%dl\n\t" \
	"movb %%dl,%2" \
	:"=&d" (__lr) \
	:"m" (*(addr)), \
	 "m" (*((addr)+6)), \
	 "0" (limit) \
        ); } while(0)

#define set_base(ldt,base) _set_base( ((char *)&(ldt)) , base )
#define set_limit(ldt,limit) _set_limit( ((char *)&(ldt)) , (limit-1)>>12 )

static inline unsigned long _get_base(char * addr)
{
	unsigned long __base;
	__asm__("movb %3,%%dh\n\t"
		"movb %2,%%dl\n\t"
		"shll $16,%%edx\n\t"
		"movw %1,%%dx"
		:"=&d" (__base)
		:"m" (*((addr)+2)),
		 "m" (*((addr)+4)),
		 "m" (*((addr)+7)));
	return __base;
}
#define get_base(ldt) _get_base( ((char *)&(ldt)) )

#define get_limit(segment) ({ \
unsigned long __limit; \
__asm__ __volatile__("lsll %1,%0\n\tincl %0":"=r" (__limit):"r" (segment)); \
__limit;})

#endif
