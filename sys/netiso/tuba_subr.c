/*
 * Copyright (c) 1992 Regents of the University of California.
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
 *	@(#)tuba_subr.c	7.4 (Berkeley) 10/15/92
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/errno.h>

#include <net/route.h>
#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_debug.h>

#include <netiso/argo_debug.h>
#include <netiso/iso.h>
#include <netiso/clnp.h>
#include <netiso/iso_pcb.h>
#include <netiso/iso_var.h>
#include <netiso/tuba_addr.h>

static struct	sockaddr_iso null_siso = { sizeof(null_siso), AF_ISO, };
extern struct	isopcb tuba_isopcb;
extern int	tuba_table_size;
extern int	tcppcbcachemiss, tcppredack, tcppreddat, tcprexmtthresh;
extern struct 	inpcb *tcp_last_inpcb;
extern struct	tcpiphdr tcp_saveti;
/*
 * Tuba initialization
 */
tuba_init()
{
#define TUBAHDRSIZE (3 /*LLC*/ + 9 /*CLNP Fixed*/ + 42 /*Addresses*/ \
		     + 6 /*CLNP Segment*/ + 20 /*TCP*/)

	tuba_isopcb.isop_next = tuba_isopcb.isop_prev = &tuba_isopcb;
	tuba_isopcb.isop_faddr = &tuba_isopcb.isop_sfaddr;
	tuba_isopcb.isop_laddr = &tuba_isopcb.isop_sladdr;
	if (max_protohdr < TUBAHDRSIZE)
		max_protohdr = TUBAHDRSIZE;
	if (max_linkhdr + TUBAHDRSIZE > MHLEN)
		panic("tuba_init");
	tuba_table_init();
}

static void
tuba_getaddr(error, sum, siso, index)
	int *error;
	u_long *sum;
	struct sockaddr_iso *siso;
	u_long index;
{
	register struct tuba_cache *tc;
	if (index < tuba_table_size && (tc = tuba_table[index])) {
		if (siso) {
			*siso = null_siso;
			siso->siso_addr = tc->tc_addr;
		}
		sum += tc->tc_sum_out;
	} else
		*error = 1;
}

tuba_output(m, tp)
	register struct mbuf *m;
	struct tcpcb *tp;
{
	struct isopcb *isop;
	register struct tcpiphdr *n;
	u_long sum, i;

	if (tp == 0 || (n = tp->t_template) == 0) {
		isop = &tuba_isopcb;
		i = sum = 0;
		n = mtod(m, struct tcpiphdr *);
		tuba_getaddr(&i, &sum, tuba_isopcb.isop_faddr,
				n->ti_dst.s_addr);
		tuba_getaddr(&i, &sum, tuba_isopcb.isop_laddr,
				n->ti_src.s_addr);
		goto adjust;
	}
	isop = (struct isopcb *)tp->t_tuba_pcb;
	if (n->ti_sum == 0) {
		i = sum = 0;
		tuba_getaddr(&i, &sum, (struct sockaddr_iso *)0,
				n->ti_dst.s_addr);
		tuba_getaddr(&i, &sum, (struct sockaddr_iso *)0,
				n->ti_src.s_addr);
		ICKSUM(sum, sum);
		n->ti_sum = sum;
		n = mtod(m, struct tcpiphdr *);
	adjust:
		if (i) {
			m_freem(m);
			return (ENOBUFS);
		}
		ICKSUM(n->ti_sum, sum + n->ti_sum);
	}
	m->m_len -= sizeof (struct ip);
	m->m_pkthdr.len -= sizeof (struct ip);
	m->m_data += sizeof (struct ip);
	return (clnp_output(m, isop, m->m_pkthdr.len, 0));
}

tuba_refcnt(isop, delta)
	struct isopcb *isop;
{
	register struct tuba_cache *tc;
	unsigned index, sum;

	if (delta != 1)
		delta = -1;
	if (isop == 0 || isop->isop_faddr == 0 || isop->isop_laddr == 0 ||
	    (delta == -1 && isop->isop_tuba_cached == 0) ||
	    (delta == 1 && isop->isop_tuba_cached != 0))
		return;
	isop->isop_tuba_cached = (delta == 1);
	if ((index = tuba_lookup(&isop->isop_sfaddr.siso_addr)) != 0 &&
	    (tc = tuba_table[index]) != 0 && (delta == 1 || tc->tc_refcnt > 0))
		tc->tc_refcnt += delta;
	if ((index = tuba_lookup(&isop->isop_sladdr.siso_addr)) != 0 &&
	    (tc = tuba_table[index]) != 0 && (delta == 1 || tc->tc_refcnt > 0))
		tc->tc_refcnt += delta;
}

tuba_pcbdetach(isop)
	struct isopcb *isop;
{
	if (isop == 0)
		return;
	tuba_refcnt(isop, -1);
	isop->isop_socket = 0;
	iso_pcbdetach(isop);
}

/*
 * Avoid  in_pcbconnect in faked out tcp_input()
 */
tuba_pcbconnect(inp, nam)
	register struct inpcb *inp;
	struct mbuf *nam;
{
	register struct sockaddr_iso *siso = mtod(nam, struct sockaddr_iso *);
	struct sockaddr_in *sin = mtod(nam, struct sockaddr_in *);
	struct tcpcb *tp = intotcpcb(inp);
	unsigned index = sin->sin_addr.s_addr;
	struct tuba_cache *tc = tuba_table[index];
	struct isopcb *isop = (struct isopcb *)tp->t_tuba_pcb;
	int error;

	inp->inp_faddr.s_addr = index;
	inp->inp_fport = sin->sin_port;
	*siso = null_siso;
	siso->siso_addr = tc->tc_addr;
	siso->siso_tlen = sizeof(inp->inp_fport);
	bcopy((caddr_t)&inp->inp_fport, TSEL(siso), sizeof(inp->inp_fport));
	nam->m_len = sizeof(*siso);
	if ((error = iso_pcbconnect(isop, nam)) == 0)
		tuba_refcnt(isop, 1);
	return (error);
}

/*
 * CALLED FROM:
 * 	clnp's input routine, indirectly through the protosw.
 * FUNCTION and ARGUMENTS:
 * Take a packet (m) from clnp, strip off the clnp header
 * and do tcp input processing.
 * No return value.  
 */
tuba_tcpinput(m, src, dst, clnp_len, ce_bit)
	register struct mbuf *m;
	struct sockaddr_iso *src, *dst;
	int clnp_len, ce_bit;
{
	int s = splnet();
	unsigned long fix_cksum, lindex, findex;
	register struct tcpiphdr *ti;
	register struct inpcb *inp;
	struct mbuf *om = 0;
	int len, tlen, off;
	register struct tcpcb *tp = 0;
	int tiflags;
	struct socket *so;
	int todrop, acked, ourfinisacked, needoutput = 0;
	short ostate;
	struct in_addr laddr;
	int dropsocket = 0, iss = 0;

	if ((m->m_flags & M_PKTHDR) == 0)
		panic("tuba_input");
	/*
	 * Do some housekeeping looking up CLNP addresses.
	 * If we are out of space might as well drop the packet now.
	 */
	tcpstat.tcps_rcvtotal++;
	if ((lindex = tuba_lookup(&dst->siso_addr) == 0) ||
	    (findex = tuba_lookup(&dst->siso_addr) == 0))
		goto drop;
	/*
	 * Get CLNP and TCP header together in first mbuf.
	 * CLNP gave us an mbuf chain WITH the clnp header pulled up,
	 * and the length of the clnp header.
	 */
	len = clnp_len + sizeof(struct tcphdr);
	if (m->m_len < len) {
		if ((m = m_pullup(m, len)) == 0) {
			tcpstat.tcps_rcvshort++;
			return;
		}
	}
	/*
	 * Calculate checksum of extended TCP header and data,
	 * by adjusting the checksum for missing parts of the header.
	 */
	m->m_data += clnp_len;
	m->m_len -= clnp_len;
	tlen = m->m_pkthdr.len -= clnp_len;
	ICKSUM(fix_cksum, tuba_table[findex]->tc_sum_in + htons((u_short)tlen)
		+ tuba_table[lindex]->tc_sum_in + in_cksum(m, tlen));
	if (fix_cksum != 0xffff) {
		tcpstat.tcps_rcvbadsum++;
		goto drop;
	}
	m->m_data -= sizeof(struct ip);
	m->m_len += sizeof(struct ip);
	m->m_pkthdr.len += sizeof(struct ip);
	/*
	 * The reassembly code assumes it will be overwriting a useless
	 * part of the packet, which is why we need to have ti point
	 * into the packet itself.
	 *
	 * Check to see if the data is properly alligned
	 * so that we can save copying the tcp header.
	 * This code knows way too much about the structure of mbufs!
	 */
	off = ((sizeof (long) - 1) & ((m->m_flags & M_EXT) ?
		(m->m_data - m->m_ext.ext_buf) :  (m->m_data - m->m_pktdat)));
	if (off) {
		struct mbuf *m0 = m_gethdr(M_DONTWAIT, MT_DATA);
		if (m0 == 0) {
			goto drop;
		}
		bcopy(mtod(m, caddr_t) + sizeof(struct ip),
		      mtod(m0, caddr_t) + sizeof(struct ip),
		      sizeof(struct tcphdr));
		m->m_data += sizeof(struct tcpiphdr);
		m0->m_next = m;
		m0->m_pkthdr = m->m_pkthdr;
		m0->m_flags = m->m_flags & M_COPYFLAGS;
		m0->m_len = sizeof(struct tcpiphdr);
		m = m0;
	}
	ti = mtod(m, struct tcpiphdr *);
	ti->ti_src.s_addr = findex;
	ti->ti_dst.s_addr = lindex;
	ti->ti_len = tlen;
	/*
	 * Now include the rest of TCP input
	 */
#define TUBA_INCLUDE
#define in_pcbconnect	tuba_pcbconnect

#include <netinet/tcp_input.c>
}
