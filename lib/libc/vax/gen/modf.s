#ifndef lint
	.asciz	"@(#)modf.s	5.1 (Berkeley) 06/05/85"
#endif not lint

/*
 * double modf (value, iptr)
 * double value, *iptr;
 *
 * Modf returns the fractional part of "value",
 * and stores the integer part indirectly through "iptr".
 */

#include "DEFS.h"

ENTRY(modf, 0)
	emodd	4(ap),$0,$0f1.0,r2,r0
	jvs	1f			# integer overflow
	cvtld	r2,*12(ap)
	ret
1:
	subd3	r0,4(ap),*12(ap)
	ret
