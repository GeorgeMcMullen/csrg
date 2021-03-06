/*-
 * Copyright (c) 1980 The Regents of the University of California.
 * All rights reserved.
 *
 * This module is believed to contain source code proprietary to AT&T.
 * Use and redistribution is subject to the Berkeley Software License
 * Agreement and your Software Agreement with AT&T (Western Electric).
 */

#ifndef lint
static char sccsid[] = "@(#)loc_.c	5.2 (Berkeley) 04/12/91";
#endif /* not lint */

/*
 * Return the address of the argument.
 *
 * calling sequence:
 *	iloc = loc (arg)
 * where:
 *	iloc will receive the address of arg
 */

long loc_(arg)
long *arg;
{
	return((long)arg);
}
