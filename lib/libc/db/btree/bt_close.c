/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Olson.
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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)bt_close.c	5.4 (Berkeley) 12/16/91";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <errno.h>
#include <db.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "btree.h"

static int bt_meta __P((BTREE *));

/*
 * BT_CLOSE -- Close a btree.
 *
 * Parameters:
 *	dbp:	pointer to access method
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS
 */
int
__bt_close(dbp)
	DB *dbp;
{
	BTREE *t;
	int fd;

	t = dbp->internal;

	/*
	 * Delete any already deleted record that we've been saving
	 * because the cursor pointed to it.
	 */
	if (ISSET(t, BTF_DELCRSR) && __bt_crsrdel(t, &t->bt_bcursor))
		return (RET_ERROR);

	if (__bt_sync(dbp) == RET_ERROR)
		return (RET_ERROR);

	if (mpool_close(t->bt_mp) == RET_ERROR)
		return (RET_ERROR);

	if (t->bt_stack)
		free(t->bt_stack);
	if (t->bt_kbuf)
		free(t->bt_kbuf);
	if (t->bt_dbuf)
		free(t->bt_dbuf);

	fd = t->bt_fd;
	free(t);
	free(dbp);
	return (close(fd) ? RET_ERROR : RET_SUCCESS);
}

/*
 * BT_SYNC -- sync the btree to disk.
 *
 * Parameters:
 *	dbp:	pointer to access method
 *
 * Returns:
 *	RET_SUCCESS, RET_ERROR.
 */
int
__bt_sync(dbp)
	const DB *dbp;
{
	BTREE *t;
	int status;
	PAGE *h;
	void *p;

	t = dbp->internal;

	if (ISSET(t, BTF_INMEM) || ISSET(t, BTF_RDONLY))
		return (RET_SUCCESS);

	if (ISSET(t, BTF_METADIRTY) && bt_meta(t) == RET_ERROR)
		return (RET_ERROR);

	/*
	 * Nastiness.  If the cursor has been marked for deletion, but not
	 * actually deleted, we have to make a copy of the page, delete the
	 * key/data item, sync the file, and then restore the original page
	 * contents.
	 */
	if (ISSET(t, BTF_DELCRSR)) {
		if ((h = mpool_get(t->bt_mp, t->bt_bcursor.pgno, 0)) == NULL)
			return (RET_ERROR);
		if ((p = malloc(t->bt_psize)) == NULL) {
			mpool_put(t->bt_mp, h, 0);
			return (RET_ERROR);
		}
		bcopy(h, p, t->bt_psize);
		if (__bt_dleaf(t, h, t->bt_bcursor.index) == RET_ERROR)
			goto ecrsr;
	}
		
	if ((status = mpool_sync(t->bt_mp)) == RET_SUCCESS)
		UNSET(t, BTF_MODIFIED);
ecrsr:	if (ISSET(t, BTF_DELCRSR)) {
		bcopy(p, h, t->bt_psize);
		free(p);
		mpool_put(t->bt_mp, h, 0);
	}
	return (status);
}

/*
 * BT_META -- write the tree meta data to disk.
 *
 * Parameters:
 *	t:	tree
 *
 * Returns:
 *	RET_ERROR, RET_SUCCESS
 */
static int
bt_meta(t)
	BTREE *t;
{
	BTMETA m;
	void *p;

	if ((p = mpool_get(t->bt_mp, P_META, 0)) == NULL)
		return (RET_ERROR);

	/* Fill in metadata -- lorder is host-independent. */
	m.m_magic = BTREEMAGIC;
	m.m_version = BTREEVERSION;
	m.m_psize = t->bt_psize;
	m.m_free = 0;           		/* XXX */
	m.m_nrecs = t->bt_nrecs;
	m.m_flags = t->bt_flags & SAVEMETA;
	m.m_lorder = htonl((u_long)t->bt_lorder);

	if (t->bt_lorder != BYTE_ORDER) {
		BLSWAP(m.m_magic);
		BLSWAP(m.m_version);
		BLSWAP(m.m_psize);
		BLSWAP(m.m_free);
		BLSWAP(m.m_nrecs);
		BLSWAP(m.m_flags);
	}

	bcopy(&m, p, sizeof(BTMETA));
	mpool_put(t->bt_mp, p, MPOOL_DIRTY);
	return (RET_SUCCESS);
}
