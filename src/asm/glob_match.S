/* ASM-GLOB: petrush_glob_match (* ? meta; [ literal). Iterativo, System V AMD64. */
#include "asm/abi.inc"

/*
 * int petrush_glob_match(const char *pat, const char *str);
 * rdi=pat, rsi=str. Retorna 1 se casa, 0 se nao.
 * Sem classes []; '[' e ']' sao literais. Sem ** especial.
 *
 * Algoritmo: backtracking de um unico '*" (star_p / star_s).
 * Caller-saved only (rax,rcx,rdx,rsi,rdi,r8,r9,r10,r11).
 */
PETRUSH_ASM_FUNC petrush_glob_match
	/* r8 = star_p (NULL), r9 = star_s */
	xorq	%r8, %r8
	xorq	%r9, %r9

.Lloop:
	/* while (*s) */
	movzbl	(%rsi), %ecx
	testb	%cl, %cl
	jz	.Ls_end

	movzbl	(%rdi), %edx		/* *p */

	cmpb	$'*', %dl
	je	.Lstar

	cmpb	$'?', %dl
	je	.Lconsume

	/* literal: *p && *p == *s */
	testb	%dl, %dl
	jz	.Lbacktrack		/* pat acabou com str restante */
	cmpb	%cl, %dl
	jne	.Lbacktrack

.Lconsume:
	incq	%rdi
	incq	%rsi
	jmp	.Lloop

.Lstar:
	/* star_p = ++p; star_s = s; colapsa '*' consecutivos */
	incq	%rdi
	movq	%rdi, %r8
	movq	%rsi, %r9
.Lstar_skip:
	movzbl	(%rdi), %edx
	cmpb	$'*', %dl
	jne	.Lstar_done
	incq	%rdi
	movq	%rdi, %r8
	jmp	.Lstar_skip
.Lstar_done:
	/* trailing '*' casa o resto */
	testb	%dl, %dl
	jz	.Lyes
	jmp	.Lloop

.Lbacktrack:
	testq	%r8, %r8
	jz	.Lno
	/* p = star_p; s = ++star_s */
	movq	%r8, %rdi
	incq	%r9
	movq	%r9, %rsi
	jmp	.Lloop

.Ls_end:
	/* while (*p == '*') p++; return !*p */
.Ltrail:
	movzbl	(%rdi), %edx
	cmpb	$'*', %dl
	jne	.Ltrail_done
	incq	%rdi
	jmp	.Ltrail
.Ltrail_done:
	testb	%dl, %dl
	jz	.Lyes
	/* fallthrough */
.Lno:
	xorl	%eax, %eax
	ret
.Lyes:
	movl	$1, %eax
	ret
PETRUSH_ASM_END petrush_glob_match

PETRUSH_NOTE_GNU_STACK
