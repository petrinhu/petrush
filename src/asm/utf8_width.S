/* ASM-UTF8: petrush_utf8_width - UAX#11 subset (System V AMD64).
 *
 * Args: rdi=s, rsi=len
 * Ret:  eax = colunas (>=0), ou -1 se UTF-8 invalido / s==NULL&&len>0
 *
 * Subset:
 *   ASCII (U+0000..U+007F)            -> 1
 *   Combining (Mn ranges listadas)    -> 0
 *   CJK / wide (W/F classico)         -> 2
 *   Demais codepoints validos         -> 1
 *   Sequencia UTF-8 invalida          -> -1 (chamada inteira)
 *
 * UTF-8 estrito: sem overlong, sem surrogates, sem >U+10FFFF, cont 10xxxxxx.
 */
#include "asm/abi.inc"

PETRUSH_ASM_FUNC petrush_utf8_width
	/* len == 0 -> 0 (s pode ser NULL) */
	testq	%rsi, %rsi
	jz	.Lzero
	/* s == NULL && len > 0 -> -1 */
	testq	%rdi, %rdi
	jz	.Lfail

	xorl	%r10d, %r10d		/* r10d = width sum */
	/* rdi = cursor, rsi = remaining */

.Lloop:
	testq	%rsi, %rsi
	jz	.Ldone

	movzbl	(%rdi), %eax		/* lead */
	/* ASCII 0xxxxxxx */
	testb	$0x80, %al
	jnz	.Lnon_ascii
	incl	%r10d
	incq	%rdi
	decq	%rsi
	jmp	.Lloop

.Lnon_ascii:
	/* continuation sozinha / lead invalido baixo: 10xxxxxx */
	movl	%eax, %ecx
	andl	$0xC0, %ecx
	cmpl	$0x80, %ecx
	je	.Lfail
	/* 2-byte: 110xxxxx (C0..DF); rejeita C0/C1 overlong */
	movl	%eax, %ecx
	andl	$0xE0, %ecx
	cmpl	$0xC0, %ecx
	je	.L2byte
	/* 3-byte: 1110xxxx */
	movl	%eax, %ecx
	andl	$0xF0, %ecx
	cmpl	$0xE0, %ecx
	je	.L3byte
	/* 4-byte: 11110xxx (F0..F4) */
	movl	%eax, %ecx
	andl	$0xF8, %ecx
	cmpl	$0xF0, %ecx
	je	.L4byte
	jmp	.Lfail

/* ---- 2-byte ---- */
.L2byte:
	cmpq	$2, %rsi
	jb	.Lfail
	cmpl	$0xC2, %eax		/* C0/C1 overlong */
	jb	.Lfail
	movzbl	1(%rdi), %edx
	movl	%edx, %ecx
	andl	$0xC0, %ecx
	cmpl	$0x80, %ecx
	jne	.Lfail
	/* cp = ((lead&0x1F)<<6) | (c1&0x3F) */
	andl	$0x1F, %eax
	shll	$6, %eax
	andl	$0x3F, %edx
	orl	%edx, %eax
	addq	$2, %rdi
	subq	$2, %rsi
	jmp	.Lclassify

/* ---- 3-byte ---- */
.L3byte:
	cmpq	$3, %rsi
	jb	.Lfail
	movzbl	1(%rdi), %edx		/* c1 */
	movzbl	2(%rdi), %r8d		/* c2 */
	/* conts */
	movl	%edx, %ecx
	andl	$0xC0, %ecx
	cmpl	$0x80, %ecx
	jne	.Lfail
	movl	%r8d, %ecx
	andl	$0xC0, %ecx
	cmpl	$0x80, %ecx
	jne	.Lfail
	/* overlong: E0 => c1 >= A0; surrogate: ED => c1 <= 9F */
	cmpl	$0xE0, %eax
	jne	.L3_ed
	cmpl	$0xA0, %edx
	jb	.Lfail
	jmp	.L3_ok
.L3_ed:
	cmpl	$0xED, %eax
	jne	.L3_ok
	cmpl	$0x9F, %edx
	ja	.Lfail
.L3_ok:
	/* cp = ((lead&0x0F)<<12) | ((c1&0x3F)<<6) | (c2&0x3F) */
	andl	$0x0F, %eax
	shll	$12, %eax
	andl	$0x3F, %edx
	shll	$6, %edx
	orl	%edx, %eax
	andl	$0x3F, %r8d
	orl	%r8d, %eax
	addq	$3, %rdi
	subq	$3, %rsi
	jmp	.Lclassify

/* ---- 4-byte ---- */
.L4byte:
	cmpq	$4, %rsi
	jb	.Lfail
	cmpl	$0xF4, %eax
	ja	.Lfail
	movzbl	1(%rdi), %edx		/* c1 */
	movzbl	2(%rdi), %r8d		/* c2 */
	movzbl	3(%rdi), %r9d		/* c3 */
	movl	%edx, %ecx
	andl	$0xC0, %ecx
	cmpl	$0x80, %ecx
	jne	.Lfail
	movl	%r8d, %ecx
	andl	$0xC0, %ecx
	cmpl	$0x80, %ecx
	jne	.Lfail
	movl	%r9d, %ecx
	andl	$0xC0, %ecx
	cmpl	$0x80, %ecx
	jne	.Lfail
	/* F0 => c1 >= 90; F4 => c1 <= 8F */
	cmpl	$0xF0, %eax
	jne	.L4_f4
	cmpl	$0x90, %edx
	jb	.Lfail
	jmp	.L4_ok
.L4_f4:
	cmpl	$0xF4, %eax
	jne	.L4_ok
	cmpl	$0x8F, %edx
	ja	.Lfail
.L4_ok:
	/* cp = ((lead&0x07)<<18) | ((c1&0x3F)<<12) | ((c2&0x3F)<<6) | (c3&0x3F) */
	andl	$0x07, %eax
	shll	$18, %eax
	andl	$0x3F, %edx
	shll	$12, %edx
	orl	%edx, %eax
	andl	$0x3F, %r8d
	shll	$6, %r8d
	orl	%r8d, %eax
	andl	$0x3F, %r9d
	orl	%r9d, %eax
	addq	$4, %rdi
	subq	$4, %rsi
	/* fallthrough .Lclassify */

/* ---- classify width of codepoint in eax ---- */
.Lclassify:
	/* Combining Mn ranges -> 0 */
	/* 0300..036F */
	cmpl	$0x0300, %eax
	jb	.Lcomb_done1
	cmpl	$0x036F, %eax
	jbe	.Ladd0
.Lcomb_done1:
	/* 1AB0..1AFF */
	cmpl	$0x1AB0, %eax
	jb	.Lcomb_done2
	cmpl	$0x1AFF, %eax
	jbe	.Ladd0
.Lcomb_done2:
	/* 1DC0..1DFF */
	cmpl	$0x1DC0, %eax
	jb	.Lcomb_done3
	cmpl	$0x1DFF, %eax
	jbe	.Ladd0
.Lcomb_done3:
	/* 20D0..20FF */
	cmpl	$0x20D0, %eax
	jb	.Lcomb_done4
	cmpl	$0x20FF, %eax
	jbe	.Ladd0
.Lcomb_done4:
	/* FE20..FE2F */
	cmpl	$0xFE20, %eax
	jb	.Lwide
	cmpl	$0xFE2F, %eax
	jbe	.Ladd0

.Lwide:
	/* UAX#11 W/F subset (mk_wcwidth-style) */
	cmpl	$0x1100, %eax
	jb	.Ladd1
	/* 1100..115F Hangul Jamo */
	cmpl	$0x115F, %eax
	jbe	.Ladd2
	/* 2329 / 232A */
	cmpl	$0x2329, %eax
	je	.Ladd2
	cmpl	$0x232A, %eax
	je	.Ladd2
	/* 2E80..A4CF except 303F */
	cmpl	$0x2E80, %eax
	jb	.Lwide2
	cmpl	$0xA4CF, %eax
	ja	.Lwide2
	cmpl	$0x303F, %eax
	je	.Ladd1
	jmp	.Ladd2
.Lwide2:
	/* AC00..D7A3 Hangul Syllables */
	cmpl	$0xAC00, %eax
	jb	.Lwide3
	cmpl	$0xD7A3, %eax
	jbe	.Ladd2
.Lwide3:
	/* F900..FAFF */
	cmpl	$0xF900, %eax
	jb	.Lwide4
	cmpl	$0xFAFF, %eax
	jbe	.Ladd2
.Lwide4:
	/* FE10..FE19 */
	cmpl	$0xFE10, %eax
	jb	.Lwide5
	cmpl	$0xFE19, %eax
	jbe	.Ladd2
.Lwide5:
	/* FE30..FE6F */
	cmpl	$0xFE30, %eax
	jb	.Lwide6
	cmpl	$0xFE6F, %eax
	jbe	.Ladd2
.Lwide6:
	/* FF00..FF60 */
	cmpl	$0xFF00, %eax
	jb	.Lwide7
	cmpl	$0xFF60, %eax
	jbe	.Ladd2
.Lwide7:
	/* FFE0..FFE6 */
	cmpl	$0xFFE0, %eax
	jb	.Lwide8
	cmpl	$0xFFE6, %eax
	jbe	.Ladd2
.Lwide8:
	/* 20000..2FFFD */
	cmpl	$0x20000, %eax
	jb	.Lwide9
	cmpl	$0x2FFFD, %eax
	jbe	.Ladd2
.Lwide9:
	/* 30000..3FFFD */
	cmpl	$0x30000, %eax
	jb	.Ladd1
	cmpl	$0x3FFFD, %eax
	jbe	.Ladd2
	jmp	.Ladd1

.Ladd0:
	jmp	.Lloop
.Ladd1:
	incl	%r10d
	jmp	.Lloop
.Ladd2:
	addl	$2, %r10d
	jmp	.Lloop

.Ldone:
	movl	%r10d, %eax
	ret
.Lzero:
	xorl	%eax, %eax
	ret
.Lfail:
	movl	$-1, %eax
	ret
PETRUSH_ASM_END petrush_utf8_width

PETRUSH_NOTE_GNU_STACK
