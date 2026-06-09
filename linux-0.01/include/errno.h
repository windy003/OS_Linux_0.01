#ifndef _ERRNO_H
#define _ERRNO_H

/*
 * 好吧，由于我没有任何其他关于可能的错误号的信息来源，
 * 我被迫使用了和 minix 相同的错误号。
 * 希望这些是符合 posix 标准或诸如此类的。我也不知道（而且
 * posix 也不会告诉我——他们想为他们那该死的标准收 $$$ 钱）。
 *
 * 我们没有使用 minix 的 _SIGN（符号）那套蹩脚做法，所以内核
 * 返回时必须自己处理好正负号。
 *
 * 注意！如果你修改了本文件，记得也要修改 strerror()！
 */

extern int errno;

#define ERROR		99
#define EPERM		 1
#define ENOENT		 2
#define ESRCH		 3
#define EINTR		 4
#define EIO		 5
#define ENXIO		 6
#define E2BIG		 7
#define ENOEXEC		 8
#define EBADF		 9
#define ECHILD		10
#define EAGAIN		11
#define ENOMEM		12
#define EACCES		13
#define EFAULT		14
#define ENOTBLK		15
#define EBUSY		16
#define EEXIST		17
#define EXDEV		18
#define ENODEV		19
#define ENOTDIR		20
#define EISDIR		21
#define EINVAL		22
#define ENFILE		23
#define EMFILE		24
#define ENOTTY		25
#define ETXTBSY		26
#define EFBIG		27
#define ENOSPC		28
#define ESPIPE		29
#define EROFS		30
#define EMLINK		31
#define EPIPE		32
#define EDOM		33
#define ERANGE		34
#define EDEADLK		35
#define ENAMETOOLONG	36
#define ENOLCK		37
#define ENOSYS		38
#define ENOTEMPTY	39

#endif
