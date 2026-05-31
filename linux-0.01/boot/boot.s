;
;	boot.s
;
; boot.s 由 BIOS 启动例程加载到 0x7c00 处，然后它把自己
; 移动到地址 0x90000 处以让出空间，并跳转到那里。
;
; 接着它利用 BIOS 中断把系统加载到 0x10000 处。此后
; 它会关闭所有中断，把系统下移到 0x0000 处，切换
; 到保护模式，并调用 system 的起始处。system 随后必须
; 在它自己的表中重新初始化保护模式，并根据需要
; 开启中断。
;
; 注意！目前 system 最长为 8*65536 字节。即使在将来，
; 这也不应成为问题。我想保持简单。这 512 kB 的
; 内核大小应该足够了——事实上更大的话就意味着我们不仅要移动
; 这些启动例程，还得对高速缓存内存（块 IO 设备）做些处理。
; 低端 640 kB 中剩下的区域就是留给它们用的。
; 不假设有其他内存是“物理的”，也就是说所有 1Mb 以上的内存
; 都采用请求调页（demand-paging）。所有 1Mb 以下的地址都保证与
; 它们的物理地址一致。
;
; 注意1：上面这段已不再完全有效。高速缓存内存既分配在 1Mb 标志
; 以上，也分配在其以下。除此之外，其内容基本正确。
;
; 注意 2！启动盘的类型必须在编译时通过设置下面的 equ 来确定。
; 让启动过程去搜寻正确的磁盘类型是严重的脑残行为。
; 加载器已被做得尽可能简单（必须如此，才能把它和切换到保护模式的
; 代码一起塞进 512 字节），连续的读错误会导致一个无法跳出的
; 死循环。请手动重启。它会尽可能一次读取整个扇区，因此加载得相当快。

; 1.44Mb 磁盘：
sectors = 18
; 1.2Mb 磁盘：
; sectors = 15
; 720kB 磁盘：
; sectors = 9

.globl begtext, begdata, begbss, endtext, enddata, endbss
.text
begtext:
.data
begdata:
.bss
begbss:
.text

BOOTSEG = 0x07c0
INITSEG = 0x9000
SYSSEG  = 0x1000			; system 被加载到 0x10000 (65536) 处。
ENDSEG	= SYSSEG + SYSSIZE

entry start
start:
	mov	ax,#BOOTSEG
	mov	ds,ax
	mov	ax,#INITSEG
	mov	es,ax
	mov	cx,#256
	sub	si,si
	sub	di,di
	rep
	movw
	jmpi	go,INITSEG
go:	mov	ax,cs
	mov	ds,ax
	mov	es,ax
	mov	ss,ax
	mov	sp,#0x400		; 任意大于 512 的值

	mov	ah,#0x03	; 读取光标位置
	xor	bh,bh
	int	0x10

	mov	cx,#24
	mov	bx,#0x0007	; 第 0 页，属性 7（正常）
	mov	bp,#msg1
	mov	ax,#0x1301	; 写字符串，并移动光标
	int	0x10

; 好，我们已经写出了消息，现在
; 我们要加载 system（到 0x10000 处）

	mov	ax,#SYSSEG
	mov	es,ax		; 0x010000 所在的段
	call	read_it
	call	kill_motor

; 如果读取顺利，我们就取得当前光标位置并把它保存起来，
; 以备后用。

	mov	ah,#0x03	; 读取光标位置
	xor	bh,bh
	int	0x10		; 保存到已知位置，con_init 会
	mov	[510],dx	; 从 0x90510 处取用它。

; 现在我们要切换到保护模式了……

	cli			; 不允许中断！

; 首先我们把 system 移动到它应在的位置

	mov	ax,#0x0000
	cld			; “方向”=0，movs 向前移动
do_move:
	mov	es,ax		; 目的段
	add	ax,#0x1000
	cmp	ax,#0x9000
	jz	end_move
	mov	ds,ax		; 源段
	sub	di,di
	sub	si,si
	mov 	cx,#0x8000
	rep
	movsw
	j	do_move

; 然后我们加载段描述符

end_move:

	mov	ax,cs		; 对了，一开始忘了这个，结果没工作 :-)
	mov	ds,ax
	lidt	idt_48		; 用 0,0 加载 idt
	lgdt	gdt_48		; 用合适的值加载 gdt

; 那很轻松，现在我们开启 A20

	call	empty_8042
	mov	al,#0xD1		; 命令写
	out	#0x64,al
	call	empty_8042
	mov	al,#0xDF		; 开启 A20
	out	#0x60,al
	call	empty_8042

; 嗯，我希望那一切顺利。现在我们得重新对中断编程了 :-(
; 我们把它们放在 intel 保留的硬件中断之后，即
; int 0x20-0x2F 处。放在那里它们不会搞乱任何东西。可惜 IBM 在
; 最初的 PC 上真的把这件事搞砸了，而且之后也一直无法纠正。
; 因此 BIOS 把中断放在 0x08-0x0f 处，而这一区间同时也被
; 内部硬件中断所使用。我们只好重新对 8259 编程，
; 而这并不好玩。

	mov	al,#0x11		; 初始化序列
	out	#0x20,al		; 发送给 8259A-1
	.word	0x00eb,0x00eb		; jmp $+2, jmp $+2
	out	#0xA0,al		; 以及发送给 8259A-2
	.word	0x00eb,0x00eb
	mov	al,#0x20		; 硬件中断的起始处（0x20）
	out	#0x21,al
	.word	0x00eb,0x00eb
	mov	al,#0x28		; 硬件中断 2 的起始处（0x28）
	out	#0xA1,al
	.word	0x00eb,0x00eb
	mov	al,#0x04		; 8259-1 是主片
	out	#0x21,al
	.word	0x00eb,0x00eb
	mov	al,#0x02		; 8259-2 是从片
	out	#0xA1,al
	.word	0x00eb,0x00eb
	mov	al,#0x01		; 两者都设为 8086 模式
	out	#0x21,al
	.word	0x00eb,0x00eb
	out	#0xA1,al
	.word	0x00eb,0x00eb
	mov	al,#0xFF		; 暂时屏蔽掉所有中断
	out	#0x21,al
	.word	0x00eb,0x00eb
	out	#0xA1,al

; 嗯，那当然不好玩 :-(。希望它能工作，反正我们也不
; 需要什么破 BIOS 了（除了最初的加载之外 :-）。
; BIOS 例程需要大量不必要的数据，而且无论如何也不那么
; “有趣”。这才是真正的程序员的做法。
;
; 好，现在到了真正进入保护模式的时候了。为了让事情
; 尽可能简单，我们不做任何寄存器设置之类的工作，
; 把那些交给 gnu 编译的 32 位程序去做。我们只是在 32 位保护模式下
; 跳转到绝对地址 0x00000 处。

	mov	ax,#0x0001	; 保护模式（PE）位
	lmsw	ax		; 就是它！
	jmpi	0,8		; 跳转到段 8（cs）的偏移 0 处

; 这个例程检查键盘命令队列是否为空。
; 没有使用超时——如果它在这里卡住，那就是机器出了问题，
; 而我们大概也无法继续了。
empty_8042:
	.word	0x00eb,0x00eb
	in	al,#0x64	; 8042 状态端口
	test	al,#2		; 输入缓冲区满了吗？
	jnz	empty_8042	; 满了——循环
	ret

; 这个例程把 system 加载到地址 0x10000 处，确保
; 不跨越任何 64kB 边界。我们尽量加载得快一些，
; 只要可能就一次加载整条磁道。
;
; 输入：es - 起始地址段（通常为 0x1000）
;
; 要适配另一种驱动器类型，这个例程必须重新编译，
; 只需修改文件开头的 "sectors" 变量
; （原本为 18，对应 1.44Mb 驱动器）
;
sread:	.word 1			; 当前磁道已读的扇区数
head:	.word 0			; 当前磁头
track:	.word 0			; 当前磁道
read_it:
	mov ax,es
	test ax,#0x0fff
die:	jne die			; es 必须位于 64kB 边界处
	xor bx,bx		; bx 是段内的起始地址
rp_read:
	mov ax,es
	cmp ax,#ENDSEG		; 我们是否已经全部加载完了？
	jb ok1_read
	ret
ok1_read:
	mov ax,#sectors
	sub ax,sread
	mov cx,ax
	shl cx,#9
	add cx,bx
	jnc ok2_read
	je ok2_read
	xor ax,ax
	sub ax,bx
	shr ax,#9
ok2_read:
	call read_track
	mov cx,ax
	add ax,sread
	cmp ax,#sectors
	jne ok3_read
	mov ax,#1
	sub ax,head
	jne ok4_read
	inc track
ok4_read:
	mov head,ax
	xor ax,ax
ok3_read:
	mov sread,ax
	shl cx,#9
	add bx,cx
	jnc rp_read
	mov ax,es
	add ax,#0x1000
	mov es,ax
	xor bx,bx
	jmp rp_read

read_track:
	push ax
	push bx
	push cx
	push dx
	mov dx,track
	mov cx,sread
	inc cx
	mov ch,dl
	mov dx,head
	mov dh,dl
	mov dl,#0
	and dx,#0x0100
	mov ah,#2
	int 0x13
	jc bad_rt
	pop dx
	pop cx
	pop bx
	pop ax
	ret
bad_rt:	mov ax,#0
	mov dx,#0
	int 0x13
	pop dx
	pop cx
	pop bx
	pop ax
	jmp read_track

/*
 * 这个过程关闭软盘驱动器的马达，
 * 这样我们就能在一个已知的状态下进入内核，
 * 之后也不必再为它操心了。
 */
kill_motor:
	push dx
	mov dx,#0x3f2
	mov al,#0
	outb
	pop dx
	ret

gdt:
	.word	0,0,0,0		; 空项（dummy）

	.word	0x07FF		; 8Mb - 限长=2047 (2048*4096=8Mb)
	.word	0x0000		; 基地址=0
	.word	0x9A00		; 代码段，可读/可执行
	.word	0x00C0		; 粒度=4096，386

	.word	0x07FF		; 8Mb - 限长=2047 (2048*4096=8Mb)
	.word	0x0000		; 基地址=0
	.word	0x9200		; 数据段，可读/可写
	.word	0x00C0		; 粒度=4096，386

idt_48:
	.word	0			; idt 限长=0
	.word	0,0			; idt 基地址=0L

gdt_48:
	.word	0x800		; gdt 限长=2048，256 个 GDT 项
	.word	gdt,0x9		; gdt 基地址 = 0X9xxxx
	
msg1:
	.byte 13,10
	.ascii "Loading system ..."
	.byte 13,10,13,10

.text
endtext:
.data
enddata:
.bss
endbss:
