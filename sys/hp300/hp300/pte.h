/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990 The Regents of the University of California.
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
 * from: Utah $Hdr: pte.h 1.11 89/09/03$
 *
 *	@(#)pte.h	7.2 (Berkeley) 12/05/90
 */

/*
 * HP300 page table entry
 *
 * There are two major kinds of pte's: those which have ever existed (and are
 * thus either now in core or on the swap device), and those which have
 * never existed, but which will be filled on demand at first reference.
 * There is a structure describing each.  There is also an ancillary
 * structure used in page clustering.
 */

#ifndef LOCORE
struct ste
{
unsigned int	sg_pfnum:20,		/* page table frame number */
		:8,			/* reserved at 0 */
		:1,			/* reserved at 1 */
		sg_prot:1,		/* write protect bit */
		sg_v:2;			/* valid bits */
};

struct pte
{
unsigned int	pg_pfnum:20,		/* page frame number or 0 */
		:3,
		pg_fod:1,		/* is fill on demand (=0) */
		:1,			/* reserved at zero */
		pg_ci:1,		/* cache inhibit bit */
		:1,			/* reserved at zero */
		pg_m:1,			/* hardware modified (dirty) bit */
		pg_u:1,			/* hardware used (reference) bit */
		pg_prot:1,		/* write protect bit */
		pg_v:2;			/* valid bit */
};

/* not used */
struct hpte
{
unsigned int	pg_pfnum:20,
		pg_high:12;		/* special for clustering */
};

struct fpte
{
unsigned int	pg_blkno:22,		/* file system block number */
		pg_fileno:1,		/* file mapped from or TEXT or ZERO */
		pg_fod:1,		/* is fill on demand (=1) */
		:6,
		pg_v:2;
};
#endif

#define	SG_V		0x00000002
#define	SG_NV		0x00000000
#define	SG_PROT		0x00000004
#define	SG_RO		0x00000004
#define	SG_RW		0x00000000
#define	SG_FRAME	0xfffff000
#define	SG_IMASK	0xffc00000
#define	SG_PMASK	0x003ff000
#define	SG_ISHIFT	22
#define	SG_PSHIFT	12

#define	PG_V		0x00000001
#define	PG_NV		0x00000000
#define	PG_PROT		0x00000004
#define	PG_U		0x00000008
#define	PG_M		0x00000010
#define	PG_FOD		0x00000100
#define	PG_RO		0x00000004
#define	PG_RW		0x00000000
#define	PG_FRAME	0xfffff000
#define	PG_CI		0x00000040
#define	PG_PFNUM(x)	(((x) & PG_FRAME) >> PGSHIFT)

/*
 * Pseudo protections.
 * Note that PG_URKW is not defined intuitively, but it is currently only
 * used in vgetu() to initialize the u-area PTEs in the process address
 * space.  Since the kernel never accesses the u-area thru these we are ok.
 */
#define	PG_KW		PG_RW
#define	PG_URKR		PG_RO
#define	PG_URKW		PG_RO
#define	PG_UW		PG_RW

#define	PG_FZERO	0
#define	PG_FTEXT	1
#define	PG_FMAX		(PG_FTEXT)

/*
 * Pte related macros
 */
#define	dirty(pte)	((pte)->pg_m)

/*
 * Kernel virtual address to page table entry and to physical address.
 */
#define	kvtopte(va) (&Sysmap[((unsigned)(va) &~ KERNBASE) >> PGSHIFT])
#define	ptetokv(pt) ((((struct pte *)(pt) - Sysmap) << PGSHIFT) | KERNBASE)
#define	kvtophys(x) ((kvtopte(x)->pg_pfnum << PGSHIFT) | ((int)(x) & PGOFSET))

#if defined(KERNEL) && !defined(LOCORE)
/* utilities defined in pmap.c */
extern	struct pte *Sysmap;
#endif /* defined(KERNEL) && !defined(LOCORE) */
