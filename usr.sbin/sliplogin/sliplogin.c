/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1990 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)sliplogin.c	5.1 (Berkeley) 06/25/90";
#endif /* not lint */

/* from static char *sccsid = "@(#)sliplogin.c	1.3	MS/ACF	89/04/18"; */

/*
 * sliplogin.c
 * [MUST BE RUN SUID, SLOPEN DOES A SUSER()!]
 *
 * This program initializes its own tty port to be an async TCP/IP interface.
 * It merely sets up the SLIP module all by its lonesome on the STREAMS stack,
 * initializes the network interface, and pauses forever waiting for hangup.
 *
 * It is a remote descendant of several similar programs with incestuous ties:
 * - Kirk Smith's slipconf, modified by Richard Johnsson @ DEC WRL.
 * - slattach, probably by Rick Adams but touched by countless hordes.
 * - the original sliplogin for 4.2bsd, Doug Kingston the mover behind it.
 * - a simple slattach-like program used to test the STREAMS SLIP code.
 *
 * There are three basic forms of usage:
 *
 * "sliplogin"
 * Invoked simply as "sliplogin" and a realuid != 0, the program looks up
 * the uid in /etc/passwd, and then the username in the file /etc/hosts.slip.
 * If and entry is found, the line on fd0 is configured for SLIP operation
 * as specified in the file.
 *
 * "sliplogin IPhost1 </dev/ttyb"
 * Invoked by root with a username, the name is looked up in the
 * /etc/hosts.slip file and if found fd0 is configured as in case 1.
 *
 * "sliplogin 192.100.1.1 192.100.1.2 255.255.255.0 < /dev/ttyb"
 * Finally, if invoked with a remote addr, local addr, and optionally
 * a net mask, the line on fd0 is setup as specified if the user is root.
 *
 * Doug Kingston 8810??		- logging + first pass at adding I_STR ioctl's
 * Rayan Zachariassen 881011	- version for SunOS STREAMS SLIP
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/termios.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/syslog.h>

#include <netinet/in.h>
#include <net/if.h>
#include <net/if_slvar.h>	/* XXX */

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <netdb.h>

#include <signal.h>
#include <strings.h>
#include <pwd.h>
#include <ttyent.h>

#define	SLIPIFNAME	"sl"

#define ADDR	1
#define MASK	2

#define	DCD_CHECK_INTERVAL 0	/* if > 0, time between automatic DCD checks */
#define	DCD_SETTLING_TIME 1	/* time between DCD change and status check */

int gotalarm = 0;
int timeleft = DCD_CHECK_INTERVAL;

#if	defined(SIGDCD) && SIGDCD > 0
void
dcd_handler()
{
#if	DCD_SETTLING_TIME > 0
	timeleft = alarm(DCD_SETTLING_TIME);
#else
	gotalarm = 1;
#endif	/* DCD_SETTLING_TIME */
}
#endif

#if DCD_CHECK_INTERVAL > 0
void
alarm_handler()
{
#ifdef SIGDCD
	if (timeleft > DCD_SETTLING_TIME)
		(void) alarm(timeleft-DCD_SETTLING_TIME);
	else
#endif /* SIGDCD */
		(void) alarm(DCD_CHECK_INTERVAL);
	gotalarm = 1;
	timeleft = 0;
}

/* Use TIOCMGET to test if DCD is low on the port of the passed descriptor */

int
lowdcd(fd)
	int fd;
{
	int mbits;

	if (ioctl(fd, TIOCMGET, (caddr_t)&mbits) < 0)
		return 1;	/* port is dead, we die */
	return !(mbits & TIOCM_CAR);
}
#endif /* DCD_CHECK_INTERVAL > 0 */

char	*Accessfile = "/etc/hosts.slip";

extern char *malloc(), *ttyname();
extern struct passwd *getpwuid();

char	*dstaddr, *localaddr, *netmask;
int	slip_mode, unit;

struct slip_modes {
	char	*sm_name;
	int	sm_value;
}	 modes[] = {
	"normal",	0,		/* slip "standard" ala Rick Adams */
	"compress",	SC_COMPRESS,	/* Van Jacobsen's tcp header comp. */
	"noicmp",	SC_NOICMP,	/* Sam's(?) ICMP suppression */
} ;

void
hup_handler(s)
	int s;
{

	syslog(LOG_INFO,
	    "%s%d: connection closed: process aborted, sig %d, remote %s\n",
	    SLIPIFNAME, unit, s, dstaddr);
	if (close(0) < 0)
		syslog(LOG_ERR, "(hup) close: %m");
	else
		syslog(LOG_INFO, "(hup) close completed");
	exit(1) ;
}

main(argc, argv)
	int argc;
	char *argv[];
{
	int	fd, s, ldisc, odisc;
	struct	termios tios, otios;
	struct	ifreq ifr;

	s = getdtablesize();
	for (fd = 3 ; fd < s ; fd++)
		close(fd);
	openlog("sliplogin", LOG_PID, LOG_DAEMON);
	if (getuid() == 0) {
		if (argc <= 1) {
			fprintf(stderr, "Usage: %s loginname\n", argv[0]);
			fprintf(stderr, "   or: %s dstaddr localaddr [mask]\n",
					argv[0]);
			exit(1);
		} else if (argc == 2) {
			findid(argv[1]);
			fprintf(stderr, "local %s remote %s mask %s\n",
				localaddr, dstaddr, netmask);
		} if (argc > 2) {
			if (argc < 3 || argc > 4) {
				fprintf(stderr,
					"Usage: %s dstaddr localaddr [mask]\n",
					argv[0]);
				exit(1);
			}
			dstaddr = argv[1];
			localaddr = argv[2];
			if (argc == 4)
				netmask = argv[3];
			else
				netmask = "default";
		}
		/*
		 * Disassociate from current controlling terminal, if any,
		 * and ensure that the slip line is our controlling terminal.
		 */
#if !defined(BSD) || BSD < 198810
		if ((fd = open("/dev/tty", O_RDONLY, 0)) >= 0) {
			(void) ioctl(fd, TIOCNOTTY, 0);
			(void) close(fd);
			/* open slip tty again to acquire as controlling tty? */
			fd = open(ttyname(0), O_RDWR, 0);
			if (fd >= 0)
				(void) close(fd);
		}
		(void) setpgrp(0, getpid());
#else
		(void) setsid();
		(void) ioctl(0, TIOCSCTTY, 0); /* not sure this will work */
#endif
	} else
		findid((char *)0);
	fchmod(0, 0600);
	/* set up the line parameters */
	if (ioctl(0, TIOCGETA, (caddr_t)&tios) < 0) {
		syslog(LOG_ERR, "ioctl (TIOCGETA): %m");
		exit(1);
	}
	otios = tios;
	tios.c_cflag = CS8|CREAD|HUPCL;
	tios.c_iflag = IGNBRK;
	tios.c_oflag = tios.c_lflag = 0;
	if (ioctl(0, TIOCSETA, (caddr_t)&tios) < 0) {
		syslog(LOG_ERR, "ioctl (TIOCSETA) (1): %m");
		exit(1);
	}
	/* find out what ldisc we started with */
	if (ioctl(0, TIOCGETD, (caddr_t)&odisc) < 0) {
		syslog(LOG_ERR, "ioctl(TIOCGETD) (1): %m");
		exit(1);
	}
	ldisc = SLIPDISC;
	if (ioctl(0, TIOCSETD, (caddr_t)&ldisc) < 0) {
		syslog(LOG_ERR, "ioctl(TIOCSETD): %m");
		exit(1);
	}
	/* find out what unit number we were assigned */
	if (ioctl(0, TIOCGETD, (caddr_t)&unit) < 0) {
		syslog(LOG_ERR, "ioctl (TIOCGETD) (2): %m");
		exit(1);
	}
	syslog(LOG_INFO, "attaching %s%d: local %s remote %s mask %s\n",
		SLIPIFNAME, unit, localaddr, dstaddr, netmask);
#ifdef notdef
	/* set the local and remote interface addresses */
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (getuid() != 0 || argc == 4) {
		(void) sprintf(ifr.ifr_name, "%s%d", SLIPIFNAME, unit);
		in_getaddr(netmask, &ifr.ifr_addr, MASK);
		if (ioctl(s, SIOCSIFNETMASK, (caddr_t)&ifr) < 0) {
			syslog(LOG_ERR, "ioctl (SIOCSIFNETMASK): %m");
			exit(1);
		}
	}
	(void) sprintf(ifr.ifr_name, "%s%d", SLIPIFNAME, unit);
	in_getaddr(dstaddr, &ifr.ifr_addr, ADDR);
	if (ioctl(s, SIOCSIFDSTADDR, (caddr_t)&ifr) < 0) {
		syslog(LOG_ERR, "ioctl (SIOCSIFDSTADDR): %m");
		exit(1);
	}
	(void) sprintf(ifr.ifr_name, "%s%d", SLIPIFNAME, unit);
	in_getaddr(localaddr, &ifr.ifr_addr, ADDR);
	/* this has the side-effect of marking the interface up */
	if (ioctl(s, SIOCSIFADDR, (caddr_t)&ifr) < 0) {
		syslog(LOG_ERR, "ioctl (SIOCSIFADDR): %m");
		exit(1);
	}
#else
	/* XXX -- give up for now and just invoke ifconfig XXX */
	{ char cmd[256];
	  sprintf(cmd, "/sbin/ifconfig %s%d inet %s %s netmask %s",
	      SLIPIFNAME, unit, localaddr, dstaddr, netmask);
	  system(cmd);
	}
#endif
	if (ioctl(0, SLIOCSFLAGS, (caddr_t)&slip_mode) < 0) {
		syslog(LOG_ERR, "ioctl (SLIOCSFLAGS): %m");
		exit(1);
	}

	/* set up signal handlers */
#if	defined(SIGDCD) && SIGDCD > 0
	(void) signal(SIGDCD, dcd_handler);
#endif
	(void) sigblock(sigmask(SIGALRM));
	(void) signal(SIGHUP, hup_handler);
	(void) signal(SIGTERM, hup_handler);

#if DCD_CHECK_INTERVAL > 0
	/* timeleft = 60 * 60 * 24 * 365 ; (void) alarm(timeleft); */
	(void) signal(SIGALRM, alarm_handler);
	(void) alarm(DCD_CHECK_INTERVAL);
#endif

	/* twiddle thumbs until we get a signal */
	while (1) {
		sigpause(0);
#if DCD_CHECK_INTERVAL > 0
		(void) sigblock(sigmask(SIGALRM));
		if (gotalarm && lowdcd(0))
			break;
		gotalarm = 0;
#endif /* DCD_CHECK_INTERVAL > 0 */
	}

#ifdef notdef
	if (lowdcd(0))
		syslog(LOG_NOTICE,
			"connection closed: loss of carrier %s%d: remote %s\n",
			SLIPIFNAME, unit, dstaddr);
#endif

	if (ioctl(0, TIOCSETD, (caddr_t)&odisc) < 0) {
		syslog(LOG_ERR, "ioctl(TIOCSETD) (2): %m");
		exit(1);
	}
	if (ioctl(0, TIOCSETA, (caddr_t)&otios) < 0) {
		syslog(LOG_ERR, "ioctl (TIOCSETA) (2): %m");
		exit(1);
	}
	if (close(0) < 0) {
		syslog(LOG_ERR, "close: %m");
		exit(1);
	}
	exit(0);
}

findid(name)
	char *name;
{
	char buf[BUFSIZ];
	static char mode[16];
	static char laddr[16];
	static char raddr[16];
	static char mask[16];
	char user[16];
	FILE *fp;
	struct passwd *pw;
	int n;

	if (name == NULL && (pw = getpwuid(getuid())) == NULL) {
		fprintf(stderr, "Your UID (%d) is unknown\n", getuid());
		syslog(LOG_ERR, "UID (%d) is unknown\n", getuid());
		exit(1);
	} else if (name == NULL)
		name = pw->pw_name;
	if ((fp = fopen(Accessfile, "r")) == NULL) {
		perror(Accessfile);
		syslog(LOG_ERR, "%s: %m\n", Accessfile);
		exit(3);
	}
	while (fgets(buf, sizeof(buf) - 1, fp)) {
		if (ferror(fp))
			break;
		n = sscanf(buf, "%15s%*[ \t]%15s%*[ \t]%15s%*[ \t]%15s%*[ \t]%15s\n",
			user, mode, laddr, raddr, mask);
		if (user[0] == '#' || n != 5)
			continue;
		if (strcmp(user, name) == 0) {
			char *p,*q; int val, i, domore;

			p = q = mode;	val = 0;
		loop:
			while (isalnum(*p)) p++;
			if(ispunct(*p) || *p == '\0') {
				if(ispunct(*p)) domore = 1; else domore = 0;
				*p++ = '\0' ; 
				for (i = 0; i <
					sizeof(modes)/sizeof(struct slip_modes)
					 ; i++) {
					if (strcmp(modes[i].sm_name, q) == 0) {
						val |= modes[i].sm_value ;
						break;
					} ;
}
				q = p;
				if(domore)goto loop;
			}

			slip_mode = val ;
			localaddr = laddr;
			dstaddr = raddr;
			netmask = mask;
			fclose(fp);
			return 0;
		}
		if (feof(fp))
			break;
	}
	fputs("SLIP access denied\n", stderr);
	syslog(LOG_ERR, "SLIP access denied for %s\n", name);
	exit(4);
}

in_getaddr(s, saddr, which)
	char *s;
	struct sockaddr *saddr;
	int which;
{
	register struct sockaddr_in *sin = (struct sockaddr_in *)saddr;
	struct hostent *hp;
	struct netent *np;
	int val;
	extern struct in_addr inet_makeaddr();
 
	bzero((caddr_t)saddr, sizeof *saddr);
	if (which == ADDR) {
		sin->sin_len = sizeof (*sin);
		sin->sin_family = AF_INET;
	} else
		sin->sin_len = 8;
	val = inet_addr(s);
	if (val != -1) {
		sin->sin_addr.s_addr = val;
		return;
	}
	hp = gethostbyname(s);
	if (hp) {
		sin->sin_family = hp->h_addrtype;
		bcopy(hp->h_addr, (char *)&sin->sin_addr, hp->h_length);
		return;
	}
	np = getnetbyname(s);
	if (np) {
		sin->sin_family = np->n_addrtype;
		sin->sin_addr = inet_makeaddr(np->n_net, INADDR_ANY);
		return;
	}
	fprintf(stderr, "sliplogin: %s: bad value\n", s);
	syslog(LOG_ERR, "%s: bad value\n", s);
	exit(1);
}
