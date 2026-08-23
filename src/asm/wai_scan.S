/* ASM-WAI: petrush_wai_scan - entrada System V AMD64.
 *
 * Inventario sysfs/proc e I/O ficam em petrush_wai_scan_impl (C, ASan).
 * Esta ilha exporta o simbolo do contrato asm.h e salta ao impl.
 * Sem root. Sem 4755. Sem serial/uuid (filtro no impl).
 */
#include "asm/abi.inc"

	.extern	petrush_wai_scan_impl

PETRUSH_ASM_FUNC petrush_wai_scan
	jmp	petrush_wai_scan_impl
PETRUSH_ASM_END petrush_wai_scan

PETRUSH_NOTE_GNU_STACK
