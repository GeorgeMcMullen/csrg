/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Added stuff to read the cmos clock on startup - Don Ahn
 *
 * Copying or redistribution in any form is explicitly forbidden
 * unless prior written permission is obtained from William Jolitz or an
 * authorized representative of the University of California, Berkeley.
 *
 * Freely redistributable copies of this code will be available in
 * the near future; for more information contact William Jolitz or
 * the Computer Systems Research Group at the University of California,
 * Berkeley.
 *
 * The name of the University may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.  THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE.
 *
 *	@(#)clock.c	5.2 (Berkeley) 06/23/90
 */

/*
 * Primitive clock interrupt routines.
 */
#include "param.h"
#include "time.h"
#include "kernel.h"
#include "icu.h"

#define DAYST 119
#define DAYEN 303

startrtclock() {

	/* initialize 8253 clock */
	outb (0x43, 0x36);
	outb (0x40, 1193182/60);
	outb (0x40, (1193182/60)/256);
}

clkreld() {
pg("clkreld");
}

/* convert 2 digit BCD number */
bcd(i)
int i;
{
	return ((i/16)*10 + (i%16));
}

/* convert years to seconds (from 1970) */
unsigned long
ytos(y)
int y;
{
	int i;
	unsigned long ret;

	ret = 0; y = y - 70;
	for(i=0;i<y;i++) {
		if (i % 4) ret += 31536000;
		else ret += 31622400;
	}
	return ret;
}

/* convert months to seconds */
unsigned long
mtos(m,leap)
int m,leap;
{
	int i;
	unsigned long ret;

	ret = 0;
	for(i=1;i<m;i++) {
		switch(i){
		case 1: case 3: case 5: case 7: case 8: case 10: case 12:
			ret += 2678400; break;
		case 4: case 6: case 9: case 11:
			ret += 2592000; break;
		case 2:
			if (leap) ret += 2505600;
			else ret += 2419200;
		}
	}
	return ret;
}


/*
 * Initialze the time of day register, based on the time base which is, e.g.
 * from a filesystem.
 */
inittodr(base)
	time_t base;
{
	unsigned long sec;
	int leap,day_week,t,yd;

	outb(0x70,9); /* year    */
	sec = bcd(inb(0x71)); leap = !(sec % 4); sec += ytos(sec);
	outb(0x70,8); /* month   */
	yd = mtos(bcd(inb(0x71)),leap); sec += yd;
	outb(0x70,7); /* date    */
	t = (bcd(inb(0x71))-1) * 86400; sec += t; yd += t;
	outb(0x70,6); /* day     */
	day_week = inb(0x71);
	outb(0x70,4); /* hour    */
	sec += bcd(inb(0x71)) * 3600;
	outb(0x70,2); /* minutes */
	sec += bcd(inb(0x71)) * 60;
	outb(0x70,0); /* seconds */
	sec += bcd(inb(0x71));

	/* XXX off by one? Need to calculate DST on SUNDAY */
	/* Perhaps we should have the RTC hold GMT time to save */
	/* us the bother of converting. */
	yd = yd / 86400;
	if ((yd >= DAYST) && ( yd <= DAYEN)) {
		sec -= 3600;
	}
	sec += tz.tz_minuteswest * 60;

	time.tv_sec = sec;
}

/*
 * Initialze the time of day register, based on the time base which is, e.g.
 * from a filesystem.
 */
test_inittodr(base)
	time_t base;
{

	outb(0x70,9); /* year    */
	printf("%d ",bcd(inb(0x71)));
	outb(0x70,8); /* month   */
	printf("%d ",bcd(inb(0x71)));
	outb(0x70,7); /* day     */
	printf("%d ",bcd(inb(0x71)));
	outb(0x70,4); /* hour    */
	printf("%d ",bcd(inb(0x71)));
	outb(0x70,2); /* minutes */
	printf("%d ",bcd(inb(0x71)));
	outb(0x70,0); /* seconds */
	printf("%d\n",bcd(inb(0x71)));

	time.tv_sec = base;
}


/*
 * Restart the clock.
 */
resettodr()
{
}

enablertclock() {
	INTREN(IRQ0);
	splnone();
}
