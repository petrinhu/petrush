/* ASM-TTY: petrush_tty_mode - RAW/COOKED via ioctl TCGETS/TCSETSF (System V AMD64).
 *
 * Args: rdi=fd, esi=mode (0=COOKED, 1=RAW)
 * Ret:  eax 0 sucesso; -errno em falha (sem tocar errno TLS)
 *       nao-TTY -> -ENOTTY; modo>1 -> -EINVAL
 *
 * Kernel termios (TCGETS): iflag/oflag/cflag/lflag + line + cc[19] = 36 bytes.
 * SYS_ioctl=16; TCGETS=0x5401; TCSETSF=0x5404 (equiv. TCSAFLUSH).
 *
 * Frame: buf 64B em -80(%rbp)..-16(%rbp); fd em -8; mode em -4.
 * (nao zeroar por cima de fd/mode.)
 */
#include "asm/abi.inc"

#define L_ECHO		0x0008
#define L_ICANON	0x0002
#define L_ISIG		0x0001
#define L_IEXTEN	0x8000
#define L_RAW_OFF	(L_ECHO|L_ICANON|L_ISIG|L_IEXTEN)

#define I_BRKINT	0x0002
#define I_ICRNL		0x0100
#define I_INPCK		0x0010
#define I_ISTRIP	0x0020
#define I_IXON		0x0400
#define I_RAW_OFF	(I_BRKINT|I_ICRNL|I_INPCK|I_ISTRIP|I_IXON)
#define I_COOKED_ON	(I_BRKINT|I_ICRNL)

#define O_OPOST		0x0001
#define C_CS8		0x0030

#define OFF_IFLAG	0
#define OFF_OFLAG	4
#define OFF_CFLAG	8
#define OFF_LFLAG	12
#define OFF_CC		17
#define VMIN_IDX	6
#define VTIME_IDX	5

#define SYS_IOCTL	16
#define TCGETS		0x5401
#define TCSETSF		0x5404
#define EINVAL_NEG	(-22)

PETRUSH_ASM_FUNC petrush_tty_mode
	cmpl	$1, %esi
	ja	.Linval

	pushq	%rbp
	movq	%rsp, %rbp
	subq	$80, %rsp

	movl	%edi, -8(%rbp)			/* fd  (fora do buf) */
	movl	%esi, -4(%rbp)			/* mode */

	/* zera so o buf 64B em -80(%rbp); nao toca -8/-4 */
	leaq	-80(%rbp), %rdi
	xorl	%eax, %eax
	movq	$8, %rcx
	rep	stosq

	/* ioctl(fd, TCGETS, &buf) */
	movl	-8(%rbp), %edi
	movl	$TCGETS, %esi
	leaq	-80(%rbp), %rdx
	movl	$SYS_IOCTL, %eax
	syscall
	testq	%rax, %rax
	js	.Lleave

	cmpl	$0, -4(%rbp)
	je	.Lcooked

	/* ---- RAW ---- */
	movl	-80+OFF_IFLAG(%rbp), %eax
	andl	$~I_RAW_OFF, %eax
	movl	%eax, -80+OFF_IFLAG(%rbp)

	movl	-80+OFF_OFLAG(%rbp), %eax
	andl	$~O_OPOST, %eax
	movl	%eax, -80+OFF_OFLAG(%rbp)

	movl	-80+OFF_CFLAG(%rbp), %eax
	orl	$C_CS8, %eax
	movl	%eax, -80+OFF_CFLAG(%rbp)

	movl	-80+OFF_LFLAG(%rbp), %eax
	andl	$~L_RAW_OFF, %eax
	movl	%eax, -80+OFF_LFLAG(%rbp)

	movb	$1, -80+OFF_CC+VMIN_IDX(%rbp)
	movb	$0, -80+OFF_CC+VTIME_IDX(%rbp)
	jmp	.Lset

.Lcooked:
	/* ---- COOKED ---- */
	movl	-80+OFF_IFLAG(%rbp), %eax
	orl	$I_COOKED_ON, %eax
	movl	%eax, -80+OFF_IFLAG(%rbp)

	movl	-80+OFF_OFLAG(%rbp), %eax
	orl	$O_OPOST, %eax
	movl	%eax, -80+OFF_OFLAG(%rbp)

	movl	-80+OFF_LFLAG(%rbp), %eax
	orl	$L_RAW_OFF, %eax
	movl	%eax, -80+OFF_LFLAG(%rbp)

.Lset:
	movl	-8(%rbp), %edi
	movl	$TCSETSF, %esi
	leaq	-80(%rbp), %rdx
	movl	$SYS_IOCTL, %eax
	syscall
	testq	%rax, %rax
	js	.Lleave
	xorl	%eax, %eax

.Lleave:
	leave
	ret

.Linval:
	movl	$EINVAL_NEG, %eax
	ret
PETRUSH_ASM_END petrush_tty_mode

PETRUSH_NOTE_GNU_STACK
