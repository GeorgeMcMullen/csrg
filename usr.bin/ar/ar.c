/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Hugh Smith at The University of Guelph.
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
static char sccsid[] = "@(#)ar.c	5.6 (Berkeley) 01/17/91";
#endif /* not lint */

#include <sys/param.h>
#include <sys/errno.h>
#include <dirent.h>
#include <stdio.h>
#include <ar.h>
#include <paths.h>
#include "archive.h"

CHDR chdr;
u_int options;
char *archive, *envtmp, *posname;

/*
 * main --
 *	main basically uses getopt to parse options and calls the appropriate
 *	functions.  Some hacks that let us be backward compatible with 4.3 ar
 *	option parsing and sanity checking.
 */
main(argc, argv)
	int argc;
	char **argv;
{
	extern int optind;
	int c, rval;
	char *p;
	int (*fcall)(), append(), contents(), delete(), extract(),
	    move(), print(), replace();
	char *rname();

	if (argc < 3)
		usage();

	/*
	 * Historic versions didn't require a '-' in front of the options.
	 * Fix it, if necessary.
	*/
	if (*argv[1] != '-') {
		if (!(p = malloc((u_int)(strlen(argv[1]) + 2)))) {
			(void)fprintf(stderr, "ar: %s.\n", strerror(errno));
			exit(1);
		}
		*p = '-';
		(void)strcpy(p + 1, argv[1]);
		argv[1] = p;
	}

	while ((c = getopt(argc, argv, "abcdilmopqrtuvx")) != EOF) {
		switch(c) {
		case 'a':
			options |= AR_A;
			break;
		case 'b':
		case 'i':
			options |= AR_B;
			break;
		case 'c':
			options |= AR_C;
			break;
		case 'd':
			options |= AR_D;
			fcall = delete;
			break;
		case 'l':		/* not documented, compatibility only */
			envtmp = ".";
			break;
		case 'm':
			options |= AR_M;
			fcall = move;
			break;
		case 'o':
			options |= AR_O;
			break;
		case 'p':
			options |= AR_P;
			fcall = print;
			break;
		case 'q':
			options |= AR_Q;
			fcall = append;
			break;
		case 'r':
			options |= AR_R;
			fcall = replace;
			break;
		case 't':
			options |= AR_T;
			fcall = contents;
			break;
		case 'u':
			options |= AR_U;
			break;
		case 'v':
			options |= AR_V;
			break;
		case 'x':
			options |= AR_X;
			fcall = extract;
			break;
		default:
			usage();
		}
	}

	argv += optind;
	argc -= optind;

	/* One of -dmpqrtx required. */
	if (!(options & (AR_D|AR_M|AR_P|AR_Q|AR_R|AR_T|AR_X))) {
		(void)fprintf(stderr,
		    "ar: one of options -dmpqrtx is required.\n");
		usage();
	}
	/* Only one of -a and -bi. */
	if (options & AR_A && options & AR_B) {
		(void)fprintf(stderr,
		    "ar: only one of -a and -[bi] options allowed.\n");
		usage();
	}
	/* -ab require a position argument. */
	if (options & (AR_A|AR_B)) {
		if (!(posname = *argv++)) {
			(void)fprintf(stderr,
			    "ar: no position operand specified.\n");
			usage();
		}
		posname = rname(posname);
	}
	/* -d only valid with -v. */
	if (options & AR_D && options & ~(AR_D|AR_V))
		badoptions("-d");
	/* -m only valid with -abiv. */
	if (options & AR_M && options & ~(AR_A|AR_B|AR_M|AR_V))
		badoptions("-m");
	/* -p only valid with -v. */
	if (options & AR_P && options & ~(AR_P|AR_V))
		badoptions("-p");
	/* -q only valid with -cv. */
	if (options & AR_Q && options & ~(AR_C|AR_Q|AR_V))
		badoptions("-q");
	/* -r only valid with -abcuv. */
	if (options & AR_R && options & ~(AR_A|AR_B|AR_C|AR_R|AR_U|AR_V))
		badoptions("-r");
	/* -t only valid with -v. */
	if (options & AR_T && options & ~(AR_T|AR_V))
		badoptions("-t");
	/* -x only valid with -ouv. */
	if (options & AR_X && options & ~(AR_O|AR_U|AR_V|AR_X))
		badoptions("-x");

	if (!(archive = *argv++)) {
		(void)fprintf(stderr, "ar: no archive specified.\n");
		usage();
	}

	/* -dmqr require a list of archive elements. */
	if (options & (AR_D|AR_M|AR_Q|AR_R) && !*argv) {
		(void)fprintf(stderr, "ar: no archive members specified.\n");
		usage();
	}

	rval = (*fcall)(argv);
	exit(rval);
}

badoptions(arg)
	char *arg;
{
	(void)fprintf(stderr,
	    "ar: illegal option combination for %s.\n", arg);
	usage();
}

usage()
{
	(void)fprintf(stderr, "usage:  ar -d [-v] archive file ...\n");
	(void)fprintf(stderr, "\tar -m [-v] archive file ...\n");
	(void)fprintf(stderr, "\tar -m [-abiv] position archive file ...\n");
	(void)fprintf(stderr, "\tar -p [-v] archive [file ...]\n");
	(void)fprintf(stderr, "\tar -q [-cv] archive file ...\n");
	(void)fprintf(stderr, "\tar -r [-cuv] archive file ...\n");
	(void)fprintf(stderr, "\tar -r [-abciuv] position archive file ...\n");
	(void)fprintf(stderr, "\tar -t [-v] archive [file ...]\n");
	(void)fprintf(stderr, "\tar -x [-ouv] archive [file ...]\n");
	exit(1);
}	
