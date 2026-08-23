/* ASM-NET: petrush_netcom_scan - entrada System V AMD64.
 *
 * Scan sysfs+netlink e I/O ficam em petrush_netcom_scan_impl (C, ASan).
 * Esta ilha exporta o simbolo do contrato asm.h e salta ao impl.
 * Sem root. Sem 4755. -up/-down fora deste simbolo.
 */
#include "asm/abi.inc"

	.extern	petrush_netcom_scan_impl

PETRUSH_ASM_FUNC petrush_netcom_scan
	jmp	petrush_netcom_scan_impl
PETRUSH_ASM_END petrush_netcom_scan

PETRUSH_NOTE_GNU_STACK
