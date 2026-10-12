| Call a STinG function (Pure C "cdecl" convention) from gcc code.
|
| STinG is built with Pure C: arguments on the stack with 16-bit ints,
| results in D0 (also pointers: cdecl), D0-D2/A0-A1 are scratch.
| gcc (without -mshort) passes 32-bit ints, so the caller packs the
| arguments into a byte buffer exactly as Pure C would push them, and
| this routine copies that buffer onto the stack.
|
|   long pc_call(void *fn, const void *args, long nbytes)    result: D0
|
| nbytes must be even.

	.text
	.globl	_pc_call

_pc_call:
	movem.l	%d2-%d7/%a2-%a6,-(%sp)	| 11 registers = 44 bytes
	move.l	%sp,%a4			| Pure C keeps A4: stack to restore
	move.l	48(%sp),%a2		| fn
	move.l	52(%sp),%a1		| args
	move.l	56(%sp),%d0		| nbytes
	lea	(%a1,%d0.l),%a1		| copy from the end: last word first
	lsr.l	#1,%d0
	bra.s	3f
2:	move.w	-(%a1),-(%sp)
3:	dbra	%d0,2b
	jsr	(%a2)
	move.l	%a4,%sp
	movem.l	(%sp)+,%d2-%d7/%a2-%a6
	rts
