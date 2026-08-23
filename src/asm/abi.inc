/* abi.inc - Macros System V AMD64 PIC (GAS / Clang .S)
 *
 * Uso nos atomos:
 *   #include "asm/abi.inc"
 *   PETRUSH_ASM_FUNC petrush_foo
 *       ...  (args: rdi, rsi, rdx, rcx, r8, r9; ret eax/rax)
 *       ret
 *   PETRUSH_ASM_END petrush_foo
 *   PETRUSH_NOTE_GNU_STACK
 *
 * PIC: enderecos locais via %rip (PETRUSH_LEA_RIP). Sem absolutos.
 * Red zone: 128 bytes abaixo de rsp; funcoes leaf podem usar sem prologue.
 * Caller-saved: rax rcx rdx rsi rdi r8 r9 r10 r11.
 * Callee-saved: rbx rbp r12 r13 r14 r15 (preservar se usados).
 */

#ifndef PETRUSH_ASM_ABI_INC
#define PETRUSH_ASM_ABI_INC

/* Prologo de simbolo exportado (texto, global, CFI, alinhado). */
.macro PETRUSH_ASM_FUNC name
	.text
	.globl	\name
	.type	\name, @function
	.balign	16
\name:
	.cfi_startproc
.endm

/* Epilogo de tamanho + CFI (chamar imediatamente apos o ultimo insn). */
.macro PETRUSH_ASM_END name
	.cfi_endproc
	.size	\name, .-\name
.endm

/*
 * Carrega endereco PIC de sym em reg destino (LEA rip-relative).
 * Ex.: PETRUSH_LEA_RIP %rax, .Ltable
 */
.macro PETRUSH_LEA_RIP dst, sym
	leaq	\sym(%rip), \dst
.endm

/* Stack nao-executavel (obrigatorio em cada TU .S linkado). */
.macro PETRUSH_NOTE_GNU_STACK
	.section .note.GNU-stack,"",@progbits
.endm

#endif /* PETRUSH_ASM_ABI_INC */
