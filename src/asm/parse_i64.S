/* ASM-I64: petrush_parse_i64 - decimal signed 64 sem overflow UB (System V AMD64).
 *
 * Args: rdi=s, rsi=len, rdx=out
 * Ret:  eax 0 = sucesso (*out escrito); -1 = vazio/invalido/overflow (*out intacto)
 * Aceita opcional '+' / '-'; so digitos ASCII; len bytes exatos.
 * Limites: INT64_MAX / INT64_MIN (magnitude via uint64, sem mul com sinal).
 */
#include "asm/abi.inc"

PETRUSH_ASM_FUNC petrush_parse_i64
	/* out == NULL -> fail */
	testq	%rdx, %rdx
	jz	.Lfail
	/* len == 0 -> empty */
	testq	%rsi, %rsi
	jz	.Lfail
	/* s == NULL com len>0 -> fail */
	testq	%rdi, %rdi
	jz	.Lfail

	movq	%rdx, %r8		/* r8 = out (salvar) */
	xorl	%r9d, %r9d		/* r9d = neg (0/1) */

	movzbl	(%rdi), %eax
	cmpb	$'+', %al
	je	.Lskip_sign
	cmpb	$'-', %al
	jne	.Lhave_digits
	movl	$1, %r9d
.Lskip_sign:
	incq	%rdi
	decq	%rsi
	jz	.Lfail			/* so sinal */

.Lhave_digits:
	testq	%rsi, %rsi
	jz	.Lfail
	/* rem_max: 7 se +, 8 se - (INT64_MAX%10 / |INT64_MIN|%10) */
	movl	$7, %r10d
	testl	%r9d, %r9d
	jz	.Lrem_ok
	movl	$8, %r10d
.Lrem_ok:
	xorq	%rcx, %rcx		/* rcx = mag (uint64) */
	movabsq	$922337203685477580, %r11 /* LIMIT/10 */

.Lloop:
	movzbl	(%rdi), %eax
	subl	$'0', %eax
	jl	.Lfail
	cmpl	$9, %eax
	jg	.Lfail
	/* overflow se mag > LIMIT/10, ou == e d > rem_max */
	cmpq	%r11, %rcx
	ja	.Lfail
	jb	.Lmul
	cmpl	%r10d, %eax
	jg	.Lfail
.Lmul:
	/* mag = mag * 10 + d  (sem overflow: ja checado) */
	leaq	(%rcx,%rcx,4), %rcx	/* mag*5 */
	addq	%rcx, %rcx		/* mag*10 */
	addq	%rax, %rcx		/* + d (eax zero-ext em rax) */
	incq	%rdi
	decq	%rsi
	jnz	.Lloop

	/* aplicar sinal; |INT64_MIN| cabe em uint64 e negq preserva 0x8000... */
	testl	%r9d, %r9d
	jz	.Lstore
	negq	%rcx
.Lstore:
	movq	%rcx, (%r8)
	xorl	%eax, %eax
	ret

.Lfail:
	movl	$-1, %eax
	ret
PETRUSH_ASM_END petrush_parse_i64

PETRUSH_NOTE_GNU_STACK
