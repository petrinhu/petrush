/* ASM-MEMEQ: petrush_memeq_ct - comparacao tempo-constante (System V AMD64).
 *
 * Args: rdi=a, rsi=b, rdx=n
 * Ret:  eax 0 = iguais, 1 = diferem
 * n==0 -> 0 (a/b podem ser NULL). Sem early-out no conteudo: XOR|OR acumula.
 */
#include "asm/abi.inc"

PETRUSH_ASM_FUNC petrush_memeq_ct
	xorl	%eax, %eax		/* acc = 0 */
	testq	%rdx, %rdx
	jz	.Ldone			/* so comprimento; OK p/ CT vs conteudo */
.Lloop:
	movzbl	(%rdi), %ecx
	movzbl	(%rsi), %r8d
	xorl	%r8d, %ecx		/* diff byte */
	orl	%ecx, %eax		/* acc |= diff */
	incq	%rdi
	incq	%rsi
	decq	%rdx
	jnz	.Lloop
	/* normaliza acc != 0 -> 1 (O(1), sem early-out no loop) */
	negl	%eax
	sbbl	%eax, %eax		/* 0 se acc era 0; -1 se nao */
	negl	%eax			/* 0 ou 1 */
.Ldone:
	ret
PETRUSH_ASM_END petrush_memeq_ct

PETRUSH_NOTE_GNU_STACK
