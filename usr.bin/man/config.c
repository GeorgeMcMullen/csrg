/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
static char sccsid[] = "@(#)config.c	8.3 (Berkeley) 01/02/94";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "pathnames.h"

struct queue_entry head;

/*
 * config --
 *
 * Read the configuration file and build a doubly linked
 * list that looks like:
 *
 *	tag1 <-> record <-> record <-> record
 *	|
 *	tag2 <-> record <-> record <-> record
 */
void
config()
{
	ENTRY *ep, *qp;
	FILE *cfp;
	size_t len;
	int lcnt;
	char *p, *t;

	if ((cfp = fopen(_PATH_MANCONF, "r")) == NULL)
		err(1, "%s", _PATH_MANCONF);
	queue_init(&head);
	for (lcnt = 1; (p = fgetline(cfp, &len)) != NULL; ++lcnt) {
		if (!len)			/* Skip empty lines. */
			continue;
		if (p[len - 1] != '\n') {	/* Skip corrupted lines. */
			warnx("%s: line %d corrupted", _PATH_MANCONF, lcnt);
			continue;
		}
		p[len - 1] = '\0';		/* Terminate the line. */

						/* Skip leading space. */
		for (; *p != '\0' && isspace(*p); ++p);
						/* Skip empty/comment lines. */
		if (*p == '\0' || *p == '#')
			continue;
						/* Find first token. */
		for (t = p; *t && !isspace(*t); ++t);
		if (*t == '\0')			/* Need more than one token.*/
			continue;
		*t = '\0';

		for (qp = head.qe_next;		/* Find any matching tag. */
		    qp != NULL && strcmp(p, qp->s); qp = qp->tags.qe_next);

		if (qp == NULL)			/* Create a new tag. */
			qp = addlist(p);

		/*
		 * Attach new records.  The keyword _build takes the rest of
		 * the line as a single entity, everything else is white
		 * space separated.  The reason we're not just using strtok(3)
		 * for all of the parsing is so we don't get caught if a line
		 * has only a single token on it.
		 */
		if (!strcmp(p, "_build")) {
			while (*++t && isspace(*t));
			if ((ep = malloc(sizeof(ENTRY))) == NULL ||
			    (ep->s = strdup(t)) == NULL)
				err(1, NULL);
			queue_enter_tail(&qp->list, ep, ENTRY *, list);
		} else for (++t; (p = strtok(t, " \t\n")) != NULL; t = NULL) {
			if ((ep = malloc(sizeof(ENTRY))) == NULL ||
			    (ep->s = strdup(p)) == NULL)
				err(1, NULL);
			queue_enter_tail(&qp->list, ep, ENTRY *, list);
		}
	}
}

/*
 * addlist --
 *	Add a tag to the list.
 */
ENTRY *
addlist(name)
	char *name;
{
	ENTRY *ep;

	if ((ep = malloc(sizeof(ENTRY))) == NULL ||
	    (ep->s = strdup(name)) == NULL)
		err(1, NULL);
	queue_init(&ep->list);
	queue_enter_head(&head, ep, ENTRY *, tags);
	return (head.qe_next);
}

/*
 * getlist --
 *	Return the linked list for a tag if it exists.
 */
ENTRY *
getlist(name)
	char *name;
{
	ENTRY *qp;

	for (qp = head.qe_next; qp != NULL; qp = qp->tags.qe_next)
		if (!strcmp(name, qp->s))
			return (qp);
	return (NULL);
}

void
debug(l)
	char *l;
{
	ENTRY *ep, *qp;

	(void)printf("%s ===============\n", l);
	for (qp = head.qe_next; qp != NULL; qp = qp->tags.qe_next) {
		printf("%s\n", qp->s);
		for (ep = qp->list.qe_next; ep != NULL; ep = ep->list.qe_next)
			printf("\t%s\n", ep->s);
	}
}
