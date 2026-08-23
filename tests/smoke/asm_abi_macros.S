/* ASM-ABI: exercita macros System V AMD64 PIC de abi.inc (so -c). */
#include "asm/abi.inc"

PETRUSH_ASM_FUNC petrush_abi_macro_probe
	xorl	%eax, %eax
	ret
PETRUSH_ASM_END petrush_abi_macro_probe

PETRUSH_NOTE_GNU_STACK
