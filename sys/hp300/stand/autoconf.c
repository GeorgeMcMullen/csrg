/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: autoconf.c 1.9 89/10/07$
 *
 *	@(#)autoconf.c	7.4 (Berkeley) 12/16/90
 */

#include "samachdep.h"
#include "sys/param.h"

#include "../dev/device.h"
#include "../dev/grfvar.h"

struct hp_hw sc_table[MAX_CTLR];

extern int internalhpib;

#if 0
#include "rominfo.h"
printrominfo()
{
	struct rominfo *rp = (struct rominfo *)ROMADDR;
	printf("boottype %x, name %s, lowram %x, sysflag %x\n",
	       rp->boottype, rp->name, rp->lowram, rp->sysflag&0xff);
	printf("rambase %x, ndrives %x, sysflag2 %x, msus %x\n",
	       rp->rambase, rp->ndrives, rp->sysflag2&0xff, rp->msus);
}
#endif

configure()
{
	find_devs();
	cninit();
#if 0
	printrominfo();
#endif
	hpibinit();
	scsiinit();
}

sctoaddr(sc)
	int sc;
{
	if (sc == -1)
		return(GRFIADDR);
	if (sc == 7 && internalhpib)
		return(internalhpib);
	if (sc < 32)
		return(0x600000+(0x10000*sc));
	if (sc >= 132 && sc < 134)
		return(0x1000000+((sc-132)*0x400000));
	return(sc);
}

/*
 * Probe all DIO select codes (0 - 32), the internal display address,.
 * and DIO-II select codes 132 (hack) and 133 (hack).
 *
 * Note that we only care about displays, SCSIs and HP-IBs.
 */
find_devs()
{
	u_char *id_reg;
	register short sc;
	register int addr;
	register struct hp_hw *hw;

	hw = sc_table;
	for (sc = -1; sc < 32; sc++) {
		addr = sctoaddr(sc);
		if (badaddr(addr))
			continue;

		id_reg = (u_char *) addr;
		hw->hw_addr = (char *) addr;
		hw->hw_id = id_reg[1] & 0xff;
		hw->hw_sc = sc;

		/*
		 * Not all internal HP-IBs respond rationally to id requests
		 * so we just go by the "internal HPIB" indicator in SYSFLAG.
		 */
		if (sc == 7 && internalhpib) {
			hw->hw_type = HPIB;
			hw++;
			continue;
		}

		switch (hw->hw_id) {
		case 5:		/* 98642A */
		case 128+5:	/* 98642A remote */
			hw->hw_type = COMMDCM;
			break;
		case 8:		/* 98625B */
		case 128:	/* 98624A */
			hw->hw_type = HPIB;
			break;
		case 57:	/* Displays */
			hw->hw_type = BITMAP;
			hw->hw_id2 = id_reg[0x15];
			switch (hw->hw_id2) {
			case 4:	/* renaissance */
			case 8: /* davinci */
				sc++;		/* occupy 2 select codes */
				break;
			}
			break;
		case 9:
			hw->hw_type = KEYBOARD;
			break;
		case 7:
		case 39:
		case 71:
		case 103:
			hw->hw_type = SCSI;
			break;
		default:	/* who cares */
			hw->hw_type = MISC;
			break;
		}
		hw++;
	}
	/*
	 * Look for displays in DIO-II space
	 */
	for (sc = 132; sc < 134; sc++) {
		addr = sctoaddr(sc);
		if (badaddr(addr))
			continue;

		id_reg = (u_char *) addr;
		hw->hw_addr = (char *) addr;
		hw->hw_id = id_reg[1] & 0xff;
		hw->hw_sc = sc;

		switch (hw->hw_id) {
		case 57:	/* Displays */
			hw->hw_type = BITMAP;
			hw->hw_id2 = id_reg[0x15];
			switch (hw->hw_id2) {
			case 4:	/* renaissance */
			case 8: /* davinci */
				sc++;		/* occupy 2 select codes */
				break;
			}
			break;
		default:	/* who cares */
			hw->hw_type = MISC;
			break;
		}
		hw++;
	}
}
