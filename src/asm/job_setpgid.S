/* ASM-PGID: petrush_job_setpgid - syscall setpgid(pid, pgid) (System V AMD64).
 *
 * Args: rdi=pid, rsi=pgid
 * Ret:  eax 0 sucesso; -errno em falha (sem tocar errno TLS)
 * SYS_setpgid = 109 (__NR_setpgid x86_64). ESRCH/EPERM saem como -errno.
 */
#include "asm/abi.inc"

PETRUSH_ASM_FUNC petrush_job_setpgid
	movl	$109, %eax		/* SYS_setpgid */
	syscall				/* clobber rcx,r11; ret rax */
	ret
PETRUSH_ASM_END petrush_job_setpgid

PETRUSH_NOTE_GNU_STACK
