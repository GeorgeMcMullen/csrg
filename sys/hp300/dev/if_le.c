/*
 * Copyright (c) 1982, 1990 The Regents of the University of California.
 * All rights reserved.
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
 *	@(#)if_le.c	7.8 (Berkeley) 08/08/91
 */

#include "le.h"
#if NLE > 0

#include "bpfilter.h"

/*
 * AMD 7990 LANCE
 *
 * This driver will generate and accept tailer encapsulated packets even
 * though it buys us nothing.  The motivation was to avoid incompatibilities
 * with VAXen, SUNs, and others that handle and benefit from them.
 * This reasoning is dubious.
 */
#include "sys/param.h"
#include "sys/systm.h"
#include "sys/mbuf.h"
#include "sys/buf.h"
#include "sys/protosw.h"
#include "sys/socket.h"
#include "sys/syslog.h"
#include "sys/ioctl.h"
#include "sys/errno.h"

#include "net/if.h"
#include "net/netisr.h"
#include "net/route.h"

#ifdef INET
#include "netinet/in.h"
#include "netinet/in_systm.h"
#include "netinet/in_var.h"
#include "netinet/ip.h"
#include "netinet/if_ether.h"
#endif

#ifdef NS
#include "netns/ns.h"
#include "netns/ns_if.h"
#endif

#ifdef ISO
extern	char all_es_snpa[], all_is_snpa[], all_l1is_snpa[], all_l2is_snpa[];
#endif

#include "../include/cpu.h"
#include "../hp300/isr.h"
#include "../include/mtpr.h"
#include "device.h"
#include "if_lereg.h"

#if NBPFILTER > 0
#include "../net/bpf.h"
#include "../net/bpfdesc.h"
char hprmp_multi[] = { 9, 0, 9, 0, 0, 4};
#endif

/* offsets for:	   ID,   REGS,    MEM,  NVRAM */
int	lestd[] = { 0, 0x4000, 0x8000, 0xC008 };

int	leattach();
struct	driver ledriver = {
	leattach, "le",
};

struct	isr le_isr[NLE];
int	ledebug = 0;		/* console error messages */

int	leintr(), leinit(), leioctl(), lestart(), ether_output();
struct	mbuf *leget();
extern	struct ifnet loif;

/*
 * Ethernet software status per interface.
 *
 * Each interface is referenced by a network interface structure,
 * le_if, which the routing code uses to locate the interface.
 * This structure contains the output queue for the interface, its address, ...
 */
struct	le_softc {
	struct	arpcom sc_ac;	/* common Ethernet structures */
#define	sc_if	sc_ac.ac_if	/* network-visible interface */
#define	sc_addr	sc_ac.ac_enaddr	/* hardware Ethernet address */
	struct	lereg0 *sc_r0;	/* DIO registers */
	struct	lereg1 *sc_r1;	/* LANCE registers */
	struct	lereg2 *sc_r2;	/* dual-port RAM */
	int	sc_rmd;		/* predicted next rmd to process */
	int	sc_runt;
	int	sc_jab;
	int	sc_merr;
	int	sc_babl;
	int	sc_cerr;
	int	sc_miss;
	int	sc_xint;
	int	sc_xown;
	int	sc_uflo;
	int	sc_rxlen;
	int	sc_rxoff;
	int	sc_txoff;
	int	sc_busy;
	short	sc_iflags;
	caddr_t sc_bpf;
	int	sc_tmd;		/* predicted next tmd to process */
	int	sc_txcnt;	/* transmissions in progress */
	int	sc_txbad;
	int	sc_txbusy;
} le_softc[NLE];

/* access LANCE registers */
#define	LERDWR(cntl, src, dst) \
	do { \
		(dst) = (src); \
	} while (((cntl)->ler0_status & LE_ACK) == 0);

/*
 * Interface exists: make available by filling in network interface
 * record.  System will initialize the interface when it is ready
 * to accept packets.
 */
leattach(hd)
	struct hp_device *hd;
{
	register struct lereg0 *ler0;
	register struct lereg2 *ler2;
	struct lereg2 *lemem = 0;
	struct le_softc *le = &le_softc[hd->hp_unit];
	struct ifnet *ifp = &le->sc_if;
	char *cp;
	int i;

	ler0 = le->sc_r0 = (struct lereg0 *)(lestd[0] + (int)hd->hp_addr);
	le->sc_r1 = (struct lereg1 *)(lestd[1] + (int)hd->hp_addr);
	ler2 = le->sc_r2 = (struct lereg2 *)(lestd[2] + (int)hd->hp_addr);
	if (ler0->ler0_id != LEID)
		return(0);
	le_isr[hd->hp_unit].isr_intr = leintr;
	hd->hp_ipl = le_isr[hd->hp_unit].isr_ipl = LE_IPL(ler0->ler0_status);
	le_isr[hd->hp_unit].isr_arg = hd->hp_unit;
	ler0->ler0_id = 0xFF;
	DELAY(100);

	/*
	 * Read the ethernet address off the board, one nibble at a time.
	 */
	cp = (char *)(lestd[3] + (int)hd->hp_addr);
	for (i = 0; i < sizeof(le->sc_addr); i++) {
		le->sc_addr[i] = (*++cp & 0xF) << 4;
		cp++;
		le->sc_addr[i] |= *++cp & 0xF;
		cp++;
	}
	printf("le%d: hardware address %s\n", hd->hp_unit,
		ether_sprintf(le->sc_addr));

	/*
	 * Setup for transmit/receive
	 */
	ler2->ler2_mode = LE_MODE;
#if defined(ISO) || NBPFILTER > 0
	ler2->ler2_ladrf0 = 0xffffffff;
	ler2->ler2_ladrf1 = 0xffffffff;
#else
	ler2->ler2_ladrf0 = 0;
	ler2->ler2_ladrf1 = 0;
#endif
	ler2->ler2_rlen = LE_RLEN;
	ler2->ler2_rdra = (int)lemem->ler2_rmd;
	ler2->ler2_tlen = LE_TLEN;
	ler2->ler2_tdra = (int)lemem->ler2_tmd;
	isrlink(&le_isr[hd->hp_unit]);
	ler0->ler0_status = LE_IE;

	ifp->if_unit = hd->hp_unit;
	ifp->if_name = "le";
	ifp->if_mtu = ETHERMTU;
	ifp->if_init = leinit;
	ifp->if_ioctl = leioctl;
	ifp->if_output = ether_output;
	ifp->if_start = lestart;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX;
#if NBPFILTER > 0
	bpfattach(&le->sc_bpf, ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif
	if_attach(ifp);
	return (1);
}

ledrinit(ler2, le)
	register struct lereg2 *ler2;
	register struct le_softc *le;
{
	register struct lereg2 *lemem = 0;
	register int i;

	ler2->ler2_padr[0] = le->sc_addr[1];
	ler2->ler2_padr[1] = le->sc_addr[0];
	ler2->ler2_padr[2] = le->sc_addr[3];
	ler2->ler2_padr[3] = le->sc_addr[2];
	ler2->ler2_padr[4] = le->sc_addr[5];
	ler2->ler2_padr[5] = le->sc_addr[4];
	for (i = 0; i < LERBUF; i++) {
		ler2->ler2_rmd[i].rmd0 = (int)lemem->ler2_rbuf[i];
		ler2->ler2_rmd[i].rmd1 = LE_OWN;
		ler2->ler2_rmd[i].rmd2 = -LEMTU;
		ler2->ler2_rmd[i].rmd3 = 0;
	}
	for (i = 0; i < LETBUF; i++) {
		ler2->ler2_tmd[i].tmd0 = (int)lemem->ler2_tbuf[i];
		ler2->ler2_tmd[i].tmd1 = 0;
		ler2->ler2_tmd[i].tmd2 = 0;
		ler2->ler2_tmd[i].tmd3 = 0;
	}
}

lereset(unit)
	register int unit;
{
	register struct le_softc *le = &le_softc[unit];
	register struct lereg0 *ler0 = le->sc_r0;
	register struct lereg1 *ler1 = le->sc_r1;
	register struct lereg2 *lemem = 0;
	register int timo = 100000;
	register int stat;

#ifdef lint
	stat = unit;
#endif
#if NBPFILTER > 0
	if (le->sc_if.if_flags & IFF_PROMISC)
		/* set the promiscuous bit */
		le->sc_r2->ler2_mode = LE_MODE|0x8000;
	else
		le->sc_r2->ler2_mode = LE_MODE;
#endif
	LERDWR(ler0, LE_CSR0, ler1->ler1_rap);
	LERDWR(ler0, LE_STOP, ler1->ler1_rdp);
	ledrinit(le->sc_r2, le);
	le->sc_txcnt = le->sc_tmd = le->sc_rmd = 0;
	LERDWR(ler0, LE_CSR1, ler1->ler1_rap);
	LERDWR(ler0, (int)&lemem->ler2_mode, ler1->ler1_rdp);
	LERDWR(ler0, LE_CSR2, ler1->ler1_rap);
	LERDWR(ler0, 0, ler1->ler1_rdp);
	LERDWR(ler0, LE_CSR0, ler1->ler1_rap);
	LERDWR(ler0, LE_INIT, ler1->ler1_rdp);
	do {
		if (--timo == 0) {
			printf("le%d: init timeout, stat = 0x%x\n",
			       unit, stat);
			break;
		}
		LERDWR(ler0, ler1->ler1_rdp, stat);
	} while ((stat & LE_IDON) == 0);
	LERDWR(ler0, LE_STOP, ler1->ler1_rdp);
	LERDWR(ler0, LE_CSR3, ler1->ler1_rap);
	LERDWR(ler0, LE_BSWP, ler1->ler1_rdp);
	LERDWR(ler0, LE_CSR0, ler1->ler1_rap);
	LERDWR(ler0, LE_STRT | LE_INEA, ler1->ler1_rdp);
	le->sc_if.if_flags &= ~IFF_OACTIVE;
}

/*
 * Initialization of interface
 */
leinit(unit)
	int unit;
{
	struct le_softc *le = &le_softc[unit];
	register struct ifnet *ifp = &le->sc_if;
	int s;

	/* not yet, if address still unknown */
	if (ifp->if_addrlist == (struct ifaddr *)0)
		return;
	if ((ifp->if_flags & IFF_RUNNING) == 0) {
		s = splimp();
		ifp->if_flags |= IFF_RUNNING;
		lereset(unit);
	        (void) lestart(ifp);
		splx(s);
	}
}

/*
 * Start output on interface.  Get another datagram to send
 * off of the interface queue, and copy it to the interface
 * before starting the output.
 */
lestart(ifp)
	struct ifnet *ifp;
{
	register struct le_softc *le = &le_softc[ifp->if_unit];
	register struct letmd *tmd;
	register struct mbuf *m;
	int len;

again:
	if ((le->sc_if.if_flags & IFF_RUNNING) == 0)
		return (0);
	IF_DEQUEUE(&le->sc_if.if_snd, m);
	if (m == 0)
		return (0);
	tmd = le->sc_r2->ler2_tmd + le->sc_tmd;
	if (tmd->tmd1 & LE_OWN)
		return (le->sc_txbusy++, 0);
	len = leput(le->sc_r2->ler2_tbuf[le->sc_tmd], m);
#if NBPFILTER > 0
	/*
	 * If bpf is listening on this interface, let it
	 * see the packet before we commit it to the wire.
	 */
	if (le->sc_bpf)
                bpf_tap(le->sc_bpf, le->sc_r2->ler2_tbuf[le->sc_tmd], len);
#endif
	tmd->tmd3 = 0;
	tmd->tmd2 = -len;
	tmd->tmd1 = LE_OWN | LE_STP | LE_ENP;
	if (++le->sc_tmd >= LETBUF)
		le->sc_tmd = 0;
	if (++le->sc_txcnt >= LETBUF) {
		le->sc_txcnt = LETBUF;
		le->sc_if.if_flags |= IFF_OACTIVE;
	} else
		goto again;
	return (0);
}

leintr(unit)
	register int unit;
{
	register struct le_softc *le = &le_softc[unit];
	register struct lereg0 *ler0 = le->sc_r0;
	register struct lereg1 *ler1;
	register int stat;

	if ((ler0->ler0_status & LE_IR) == 0)
		return(0);
	if (ler0->ler0_status & LE_JAB) {
		le->sc_jab++;
		lereset(unit);
		return(1);
	}
	ler1 = le->sc_r1;
	LERDWR(ler0, ler1->ler1_rdp, stat);
	if (stat & LE_SERR) {
		leerror(unit, stat);
		if (stat & LE_MERR) {
			le->sc_merr++;
			lereset(unit);
			return(1);
		}
		if (stat & LE_BABL)
			le->sc_babl++;
		if (stat & LE_CERR)
			le->sc_cerr++;
		if (stat & LE_MISS)
			le->sc_miss++;
		LERDWR(ler0, LE_BABL|LE_CERR|LE_MISS|LE_INEA, ler1->ler1_rdp);
	}
	if ((stat & LE_RXON) == 0) {
		le->sc_rxoff++;
		lereset(unit);
		return(1);
	}
	if ((stat & LE_TXON) == 0) {
		le->sc_txoff++;
		lereset(unit);
		return(1);
	}
	if (stat & LE_RINT) {
		/* interrupt is cleared in lerint */
		lerint(unit);
	}
	if (stat & LE_TINT) {
		LERDWR(ler0, LE_TINT|LE_INEA, ler1->ler1_rdp);
		lexint(unit);
	}
	return(1);
}

/*
 * Ethernet interface transmitter interrupt.
 * Start another output if more data to send.
 */
lexint(unit)
	register int unit;
{
	register struct le_softc *le = &le_softc[unit];
	register struct letmd *tmd;
	int i, loopcount = 0;

	if ((le->sc_if.if_flags & IFF_OACTIVE) == 0) {
		le->sc_xint++;
		return;
	}
again:
	if ((i = le->sc_tmd - le->sc_txcnt) < 0) i += LETBUF;
	tmd = le->sc_r2->ler2_tmd + i;
	if (tmd->tmd1 & LE_OWN) {
		if (loopcount)
			goto out;
		le->sc_xown++;
		return;
	}
	if (tmd->tmd1 & LE_ERR) {
err:
		lexerror(unit);
		le->sc_if.if_oerrors++;
		if (tmd->tmd3 & (LE_TBUFF|LE_UFLO)) {
			le->sc_uflo++;
			lereset(unit);
		}
		else if (tmd->tmd3 & LE_LCOL)
			le->sc_if.if_collisions++;
		else if (tmd->tmd3 & LE_RTRY)
			le->sc_if.if_collisions += 16;
	}
	else if (tmd->tmd3 & LE_TBUFF)
		/* XXX documentation says BUFF not included in ERR */
		goto err;
	else if (tmd->tmd1 & LE_ONE)
		le->sc_if.if_collisions++;
	else if (tmd->tmd1 & LE_MORE)
		/* what is the real number? */
		le->sc_if.if_collisions += 2;
	else
		le->sc_if.if_opackets++;
	loopcount++;
	if (--le->sc_txcnt > 0)
		goto again;
	if (le->sc_txcnt < 0) {
		le->sc_txbad++;
		le->sc_txcnt = 0;
	}
out:
	le->sc_if.if_flags &= ~IFF_OACTIVE;
	(void) lestart(&le->sc_if);
}

#define	LENEXTRMP \
	if (++bix == LERBUF) bix = 0, rmd = le->sc_r2->ler2_rmd; else ++rmd

/*
 * Ethernet interface receiver interrupt.
 * If input error just drop packet.
 * Decapsulate packet based on type and pass to type specific
 * higher-level input routine.
 */
lerint(unit)
	int unit;
{
	register struct le_softc *le = &le_softc[unit];
	register int bix = le->sc_rmd;
	register struct lermd *rmd = &le->sc_r2->ler2_rmd[bix];

	/*
	 * Out of sync with hardware, should never happen?
	 */
	if (rmd->rmd1 & LE_OWN) {
		LERDWR(le->sc_r0, LE_RINT|LE_INEA, le->sc_r1->ler1_rdp);
		return;
	}

	/*
	 * Process all buffers with valid data
	 */
	while ((rmd->rmd1 & LE_OWN) == 0) {
		int len = rmd->rmd3;

		/* Clear interrupt to avoid race condition */
		LERDWR(le->sc_r0, LE_RINT|LE_INEA, le->sc_r1->ler1_rdp);

		if (rmd->rmd1 & LE_ERR) {
			le->sc_rmd = bix;
			lererror(unit, "bad packet");
			le->sc_if.if_ierrors++;
		} else if ((rmd->rmd1 & (LE_STP|LE_ENP)) != (LE_STP|LE_ENP)) {
			/*
			 * Find the end of the packet so we can see how long
			 * it was.  We still throw it away.
			 */
			do {
				LERDWR(le->sc_r0, LE_RINT|LE_INEA,
				       le->sc_r1->ler1_rdp);
				rmd->rmd3 = 0;
				rmd->rmd1 = LE_OWN;
				LENEXTRMP;
			} while (!(rmd->rmd1 & (LE_OWN|LE_ERR|LE_STP|LE_ENP)));
			le->sc_rmd = bix;
			lererror(unit, "chained buffer");
			le->sc_rxlen++;
			/*
			 * If search terminated without successful completion
			 * we reset the hardware (conservative).
			 */
			if ((rmd->rmd1 & (LE_OWN|LE_ERR|LE_STP|LE_ENP)) !=
			    LE_ENP) {
				lereset(unit);
				return;
			}
		} else
			leread(unit, le->sc_r2->ler2_rbuf[bix], len);
		rmd->rmd3 = 0;
		rmd->rmd1 = LE_OWN;
		LENEXTRMP;
	}
	le->sc_rmd = bix;
}

leread(unit, buf, len)
	int unit;
	char *buf;
	int len;
{
	register struct le_softc *le = &le_softc[unit];
	register struct ether_header *et;
    	struct mbuf *m;
	int off, resid;

	le->sc_if.if_ipackets++;
	et = (struct ether_header *)buf;
	et->ether_type = ntohs((u_short)et->ether_type);
	/* adjust input length to account for header and CRC */
	len = len - sizeof(struct ether_header) - 4;

#define	ledataaddr(et, off, type)	((type)(((caddr_t)((et)+1)+(off))))
	if (et->ether_type >= ETHERTYPE_TRAIL &&
	    et->ether_type < ETHERTYPE_TRAIL+ETHERTYPE_NTRAILER) {
		off = (et->ether_type - ETHERTYPE_TRAIL) * 512;
		if (off >= ETHERMTU)
			return;		/* sanity */
		et->ether_type = ntohs(*ledataaddr(et, off, u_short *));
		resid = ntohs(*(ledataaddr(et, off+2, u_short *)));
		if (off + resid > len)
			return;		/* sanity */
		len = off + resid;
	} else
		off = 0;

	if (len <= 0) {
		if (ledebug)
			log(LOG_WARNING,
			    "le%d: ierror(runt packet): from %s: len=%d\n",
			    unit, ether_sprintf(et->ether_shost), len);
		le->sc_runt++;
		le->sc_if.if_ierrors++;
		return;
	}
#if NBPFILTER > 0
	/*
	 * Check if there's a bpf filter listening on this interface.
	 * If so, hand off the raw packet to bpf, which must deal with
	 * trailers in its own way.
	 */
	if (le->sc_bpf)
		bpf_tap(le->sc_bpf, buf, len + sizeof(struct ether_header));
#endif
#if defined(ISO) || NBPFILTER > 0
	/*
	 * Note that the interface cannot be in promiscuous mode if
	 * there are no bpf listeners.  If we are in promiscuous
	 * mode, we have to check if this packet is really ours.
	 * However, there may be appropriate multicate addresses involved
	 */
#define NOT_TO(p) (bcmp(et->ether_dhost, p, sizeof(et->ether_dhost)) != 0)
	if (et->ether_dhost[0] & 1) {
		if (NOT_TO(etherbroadcastaddr) && NOT_TO(hprmp_multi)
#ifdef ISO
		    && NOT_TO(all_es_snpa) && NOT_TO(all_is_snpa)
		    && NOT_TO(all_l1is_snpa) && NOT_TO(all_l2is_snpa)
#endif
		     ) return;
	} else if ((le->sc_if.if_flags & IFF_PROMISC) && NOT_TO(le->sc_addr))
		return;
#endif
	/*
	 * Pull packet off interface.  Off is nonzero if packet
	 * has trailing header; leget will then force this header
	 * information to be at the front, but we still have to drop
	 * the type and length which are at the front of any trailer data.
	 */
	m = leget(buf, len, off, &le->sc_if);
	if (m == 0)
		return;
	ether_input(&le->sc_if, et, m);
}

/*
 * Routine to copy from mbuf chain to transmit
 * buffer in board local memory.
 */
leput(lebuf, m)
	register char *lebuf;
	register struct mbuf *m;
{
	register struct mbuf *mp;
	register int len, tlen = 0;

	for (mp = m; mp; mp = mp->m_next) {
		len = mp->m_len;
		if (len == 0)
			continue;
		tlen += len;
		bcopy(mtod(mp, char *), lebuf, len);
		lebuf += len;
	}
	m_freem(m);
	if (tlen < LEMINSIZE) {
		bzero(lebuf, LEMINSIZE - tlen);
		tlen = LEMINSIZE;
	}
	return(tlen);
}

/*
 * Routine to copy from board local memory into mbufs.
 */
struct mbuf *
leget(lebuf, totlen, off0, ifp)
	char *lebuf;
	int totlen, off0;
	struct ifnet *ifp;
{
	register struct mbuf *m;
	struct mbuf *top = 0, **mp = &top;
	register int off = off0, len;
	register char *cp;
	char *epkt;

	lebuf += sizeof (struct ether_header);
	cp = lebuf;
	epkt = cp + totlen;
	if (off) {
		cp += off + 2 * sizeof(u_short);
		totlen -= 2 * sizeof(u_short);
	}

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0)
		return (0);
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = totlen;
	m->m_len = MHLEN;

	while (totlen > 0) {
		if (top) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == 0) {
				m_freem(top);
				return (0);
			}
			m->m_len = MLEN;
		}
		len = min(totlen, epkt - cp);
		if (len >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if (m->m_flags & M_EXT)
				m->m_len = len = min(len, MCLBYTES);
			else
				len = m->m_len;
		} else {
			/*
			 * Place initial small packet/header at end of mbuf.
			 */
			if (len < m->m_len) {
				if (top == 0 && len + max_linkhdr <= m->m_len)
					m->m_data += max_linkhdr;
				m->m_len = len;
			} else
				len = m->m_len;
		}
		bcopy(cp, mtod(m, caddr_t), (unsigned)len);
		cp += len;
		*mp = m;
		mp = &m->m_next;
		totlen -= len;
		if (cp == epkt)
			cp = lebuf;
	}
	return (top);
}

/*
 * Process an ioctl request.
 */
leioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	int cmd;
	caddr_t data;
{
	register struct ifaddr *ifa = (struct ifaddr *)data;
	struct le_softc *le = &le_softc[ifp->if_unit];
	struct lereg1 *ler1 = le->sc_r1;
	int s = splimp(), error = 0;

	switch (cmd) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			leinit(ifp->if_unit);	/* before arpwhohas */
			((struct arpcom *)ifp)->ac_ipaddr =
				IA_SIN(ifa)->sin_addr;
			arpwhohas((struct arpcom *)ifp, &IA_SIN(ifa)->sin_addr);
			break;
#endif
#ifdef NS
		case AF_NS:
		    {
			register struct ns_addr *ina = &(IA_SNS(ifa)->sns_addr);

			if (ns_nullhost(*ina))
				ina->x_host = *(union ns_host *)(le->sc_addr);
			else {
				/* 
				 * The manual says we can't change the address 
				 * while the receiver is armed,
				 * so reset everything
				 */
				ifp->if_flags &= ~IFF_RUNNING; 
				LERDWR(le->sc_r0, LE_STOP, ler1->ler1_rdp);
				bcopy((caddr_t)ina->x_host.c_host,
				    (caddr_t)le->sc_addr, sizeof(le->sc_addr));
			}
			leinit(ifp->if_unit); /* does le_setaddr() */
			break;
		    }
#endif
		default:
			leinit(ifp->if_unit);
			break;
		}
		break;

	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    ifp->if_flags & IFF_RUNNING) {
			LERDWR(le->sc_r0, LE_STOP, ler1->ler1_rdp);
			ifp->if_flags &= ~IFF_RUNNING;
		} else if (ifp->if_flags & IFF_UP &&
		    (ifp->if_flags & IFF_RUNNING) == 0)
			leinit(ifp->if_unit);
		/*
		 * If the state of the promiscuous bit changes, the interface
		 * must be reset to effect the change.
		 */
		if (((ifp->if_flags ^ le->sc_iflags) & IFF_PROMISC) &&
		    (ifp->if_flags & IFF_RUNNING)) {
			le->sc_iflags = ifp->if_flags;
			lereset(ifp->if_unit);
			lestart(ifp);
		}
		break;

	default:
		error = EINVAL;
	}
	splx(s);
	return (error);
}

leerror(unit, stat)
	int unit;
	int stat;
{
	if (!ledebug)
		return;

	/*
	 * Not all transceivers implement heartbeat
	 * so we only log CERR once.
	 */
	if ((stat & LE_CERR) && le_softc[unit].sc_cerr)
		return;
	log(LOG_WARNING,
	    "le%d: error: stat=%b\n", unit,
	    stat,
	    "\20\20ERR\17BABL\16CERR\15MISS\14MERR\13RINT\12TINT\11IDON\10INTR\07INEA\06RXON\05TXON\04TDMD\03STOP\02STRT\01INIT");
}

lererror(unit, msg)
	int unit;
	char *msg;
{
	register struct le_softc *le = &le_softc[unit];
	register struct lermd *rmd;
	int len;

	if (!ledebug)
		return;

	rmd = &le->sc_r2->ler2_rmd[le->sc_rmd];
	len = rmd->rmd3;
	log(LOG_WARNING,
	    "le%d: ierror(%s): from %s: buf=%d, len=%d, rmd1=%b\n",
	    unit, msg,
	    len > 11 ? ether_sprintf(&le->sc_r2->ler2_rbuf[le->sc_rmd][6]) : "unknown",
	    le->sc_rmd, len,
	    rmd->rmd1,
	    "\20\20OWN\17ERR\16FRAM\15OFLO\14CRC\13RBUF\12STP\11ENP");
}

lexerror(unit)
	int unit;
{
	register struct le_softc *le = &le_softc[unit];
	register struct letmd *tmd;
	int len;

	if (!ledebug)
		return;

	tmd = le->sc_r2->ler2_tmd;
	len = -tmd->tmd2;
	log(LOG_WARNING,
	    "le%d: oerror: to %s: buf=%d, len=%d, tmd1=%b, tmd3=%b\n",
	    unit,
	    len > 5 ? ether_sprintf(&le->sc_r2->ler2_tbuf[0][0]) : "unknown",
	    0, len,
	    tmd->tmd1,
	    "\20\20OWN\17ERR\16RES\15MORE\14ONE\13DEF\12STP\11ENP",
	    tmd->tmd3,
	    "\20\20BUFF\17UFLO\16RES\15LCOL\14LCAR\13RTRY");
}
#endif
