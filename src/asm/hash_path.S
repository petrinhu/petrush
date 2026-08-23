/* ASM-HASH: petrush_hash_path - FNV-1a 64-bit, System V AMD64.
 *
 * Args: rdi=path, rsi=len
 * Ret:  rax = hash
 * Semente: 14695981039346656037 (0xcbf29ce484222325)
 * Prime:   1099511628211 (0x100000001b3)
 * len==0 -> devolve a semente (path pode ser NULL).
 * Nao exige NUL; processa exactamente len bytes.
 */
#include "asm/abi.inc"

PETRUSH_ASM_FUNC petrush_hash_path
	movabsq	$0xcbf29ce484222325, %rax	/* offset basis */
	testq	%rsi, %rsi
	jz	.Ldone
	movabsq	$0x100000001b3, %rdx		/* FNV prime */
.Lloop:
	movzbl	(%rdi), %ecx
	xorq	%rcx, %rax
	imulq	%rdx, %rax			/* wrap 64-bit */
	incq	%rdi
	decq	%rsi
	jnz	.Lloop
.Ldone:
	ret
PETRUSH_ASM_END petrush_hash_path

PETRUSH_NOTE_GNU_STACK
