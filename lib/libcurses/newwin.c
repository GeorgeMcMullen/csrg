/*
 * Copyright (c) 1981 Regents of the University of California.
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
static char sccsid[] = "@(#)newwin.c	5.11 (Berkeley) 11/11/92";
#endif	/* not lint */

#include <curses.h>
#include <stdlib.h>

#undef	nl		/* Don't need it here, and it interferes. */

static WINDOW 	*makenew __P((int, int, int, int));
static void	 __swflags __P((WINDOW *));

void	 __set_subwin __P((WINDOW *, WINDOW *));

/*
 * newwin --
 *	Allocate space for and set up defaults for a new window.
 */
WINDOW *
newwin(nl, nc, by, bx)
	register int nl, nc, by, bx;
{
	register WINDOW *win;
	register __LINE *lp;
	register int  i, j;
	register __LDATA *sp;

	if (nl == 0)
		nl = LINES - by;
	if (nc == 0)
		nc = COLS - bx;

	if ((win = makenew(nl, nc, by, bx)) == NULL)
		return (NULL);

	win->nextp = win;
	win->ch_off = 0;

#ifdef DEBUG
	__TRACE("newwin: win->ch_off = %d\n", win->ch_off);
#endif

	for (lp = win->lines[0], i = 0; i < nl; i++, lp = win->lines[i]) {
		lp->flags = 0;
		for (sp = lp->line, j = 0; j < nc; j++, sp++) {
			sp->ch = ' ';
			sp->attr = 0;
		}
		lp->hash = __hash(lp->line, nc * __LDATASIZE);
	}
	return (win);
}

WINDOW *
subwin(orig, nl, nc, by, bx)
	register WINDOW *orig;
	register int by, bx, nl, nc;
{
	register WINDOW *win;

	/* Make sure window fits inside the original one. */
#ifdef	DEBUG
	__TRACE("subwin: (%0.2o, %d, %d, %d, %d)\n", orig, nl, nc, by, bx);
#endif
	if (by < orig->begy || bx < orig->begx
	    || by + nl > orig->maxy + orig->begy
	    || bx + nc > orig->maxx + orig->begx)
		return (NULL);
	if (nl == 0)
		nl = orig->maxy + orig->begy - by;
	if (nc == 0)
		nc = orig->maxx + orig->begx - bx;
	if ((win = makenew(nl, nc, by, bx)) == NULL)
		return (NULL);
	win->nextp = orig->nextp;
	orig->nextp = win;
	win->orig = orig;
	__set_subwin(orig, win);
	return (win);
}

/*
 * This code is shared with mvwin().
 */
void
__set_subwin(orig, win)
	register WINDOW *orig, *win;
{
	register int j, k, ocnt, cnt;
	register __LINE *lp, *olp;

	j = win->begy - orig->begy;
	k = win->begx - orig->begx;
	win->ch_off = k;
#ifdef DEBUG
	__TRACE("__set_subwin: win->ch_off = %d\n", win->ch_off);
#endif

	for (ocnt = j, cnt = 0; ocnt < orig->maxy && cnt < win->maxy; 
	     cnt++, ocnt++) {
		olp = orig->lines[ocnt];
		lp = win->lines[cnt];
		lp->firstchp = olp->firstchp;
		lp->lastchp = olp->lastchp;
		lp->line = &olp->line[k];
		lp->flags = olp->flags;
		cnt++;
	}
}

/*
 * makenew --
 *	Set up a window buffer and returns a pointer to it.
 */
static WINDOW *
makenew(nl, nc, by, bx)
	register int by, bx, nl, nc;
{
	register WINDOW *win;
	register __LINE *lp;
	int i;
	

#ifdef	DEBUG
	__TRACE("makenew: (%d, %d, %d, %d)\n", nl, nc, by, bx);
#endif
	if ((win = malloc(sizeof(*win))) == NULL)
		return (NULL);
#ifdef DEBUG
	__TRACE("makenew: nl = %d\n", nl);
#endif

	/* 
 	 * Set up line pointer array and line space.
         */
	if ((win->lines = malloc (nl * sizeof(__LINE *))) == NULL) {
		free(win);
		return NULL;
	}
	if ((win->lspace = malloc (nl * sizeof(__LINE))) == NULL) {
		free (win);
		free (win->lines);
		return NULL;
	}

	/*
	 * Allocate window space in one chunk.
	 */
	if ((win->wspace = malloc(nc * nl * sizeof(__LDATA))) == NULL) {
		free(win->lines);
		free(win->lspace);
		free(win);
		return NULL;
	}

	/*
	 * Point line pointers to line space, and lines themselves into
	 * window space.
	 */
	for (lp = win->lspace, i = 0; i < nl; i++, lp++) {
		win->lines[i] = lp;
	        lp->line = &win->wspace[i * nc];
		lp->firstchp = &lp->firstch;
		lp->lastchp = &lp->lastch;
		lp->firstch = 0;
		lp->lastch = 0;
	}

#ifdef DEBUG
	__TRACE("makenew: nc = %d\n", nc);
#endif
	win->cury = win->curx = 0;
	win->maxy = nl;
	win->maxx = nc;

	win->begy = by;
	win->begx = bx;
	win->flags = 0;
	__swflags(win);
#ifdef DEBUG
	__TRACE("makenew: win->flags = %0.2o\n", win->flags);
	__TRACE("makenew: win->maxy = %d\n", win->maxy);
	__TRACE("makenew: win->maxx = %d\n", win->maxx);
	__TRACE("makenew: win->begy = %d\n", win->begy);
	__TRACE("makenew: win->begx = %d\n", win->begx);
#endif
	return (win);
}

static void
__swflags(win)
	register WINDOW *win;
{
	win->flags &= 
	    ~(__ENDLINE | __FULLLINE | __FULLWIN | __SCROLLWIN | __LEAVEOK);
	if (win->begx + win->maxx == COLS) {
		win->flags |= __ENDLINE;
		if (win->begx == 0) {
			if (AL && DL)
				win->flags |= __FULLLINE;
			if (win->maxy == LINES && win->begy == 0)
				win->flags |= __FULLWIN;
		}
		if (win->begy + win->maxy == LINES)
			win->flags |= __SCROLLWIN;
	}
}
