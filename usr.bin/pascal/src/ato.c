/* Copyright (c) 1979 Regents of the University of California */

#ifndef lint
static char sccsid[] = "@(#)ato.c 1.5 09/19/83";
#endif
/* This is an sccs experiment */

#include "whoami.h"
#include "0.h"

long
a8tol(cp)
	char *cp;
{
	int err;
	long l;
	register CHAR c;

	l = 0;
	err = 0;
	while ((c = *cp++) != '\0') {
		if (c == '8' || c == '9')
			if (err == 0) {
				error("8 or 9 in octal number");
				err++;
			}
		c -= '0';
		if ((l & 016000000000L) != 0)
			if (err == 0) {
				error("Number too large for this implementation");
				err++;
			}
		l = (l << 3) | c;
	}
	return (l);
}

/*
 * Note that the version of atof
 * used in this compiler does not
 * (sadly) complain when floating
 * point numbers are too large.
 */
