/*-
 * Copyright (c) 1991 The Regents of the University of California.
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
"@(#) Copyright (c) 1991 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)pwd_mkdb.c	5.2 (Berkeley) 03/08/91";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <ndbm.h>
#include <pwd.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define	INSECURE	1
#define	SECURE		2
#define	PERM_INSECURE	(S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)
#define	PERM_SECURE	(S_IRUSR|S_IWUSR)

char *progname = "pwd_mkdb";

static enum state { FILE_INSECURE, FILE_SECURE, FILE_ORIG } clean;
static struct passwd pwd;			/* password structure */
static char *pname;				/* password file name */

main(argc, argv)
	int argc;
	char **argv;
{
	extern int optind;
	register int len, makeold;
	register char *p, *t;
	FILE *fp, *oldfp;
	DBM *dp, *edp;
	sigset_t set;
	datum key, data;
	int ch, cnt, tfd;
	char buf[MAX(MAXPATHLEN, LINE_MAX * 2)], tbuf[1024];

	makeold = 0;
	while ((ch = getopt(argc, argv, "pv")) != EOF)
		switch(ch) {
		case 'p':			/* create V7 "file.orig" */
			makeold = 1;
			break;
		case 'v':			/* backward compatible */
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	/*
	 * This could be done to allow the user to interrupt.  Probably
	 * not worth the effort.
	 */
	sigemptyset(&set);
	sigaddset(&set, SIGTSTP);
	sigaddset(&set, SIGHUP);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGQUIT);
	sigaddset(&set, SIGTERM);
	(void)sigprocmask(SIG_BLOCK, &set, (sigset_t *)NULL);

	pname = *argv;
	/* Open the original password file */
	if (!(fp = fopen(pname, "r")))
		error(pname);

	/* Open the password database. */
	(void)sprintf(buf, "%s.tmp", _PATH_MP_DB);
	if (!(dp = dbm_open(buf, O_WRONLY|O_CREAT|O_EXCL, PERM_INSECURE)))
		error(buf);
	clean = FILE_INSECURE;

	/* Open the encrypted password database. */
	(void)sprintf(buf, "%s.tmp", _PATH_SMP_DB);
	if (!(edp = dbm_open(buf, O_WRONLY|O_CREAT|O_EXCL, PERM_SECURE)))
		error(buf);
	clean = FILE_SECURE;

	/*
	 * Open file for old password file.  Minor trickiness -- don't want to
	 * chance the file already existing, since someone (stupidly) might
	 * still be using this for permission checking.  So, open it first and
	 * fdopen the resulting fd.  Don't really care who reads it.
	 */
	if (makeold) {
		(void)sprintf(buf, "%s.orig", pname);
		if ((tfd = open(buf,
		    O_WRONLY|O_CREAT|O_EXCL, PERM_INSECURE)) < 0)
			error(buf);
		if (!(oldfp = fdopen(tfd, "w")))
			error(buf);
		clean = FILE_ORIG;
	}

	data.dptr = buf;
	key.dptr = tbuf;
	for (cnt = 1; scan(fp, &pwd); ++cnt) {
#define	COMPACT(e)	t = e; while (*p++ = *t++);
		/* Create insecure data. */
		p = buf;
		COMPACT(pwd.pw_name);
		COMPACT("*");
		bcopy((char *)&pwd.pw_uid, p, sizeof(int));
		p += sizeof(int);
		bcopy((char *)&pwd.pw_gid, p, sizeof(int));
		p += sizeof(int);
		bcopy((char *)&pwd.pw_change, p, sizeof(time_t));
		p += sizeof(time_t);
		COMPACT(pwd.pw_class);
		COMPACT(pwd.pw_gecos);
		COMPACT(pwd.pw_dir);
		COMPACT(pwd.pw_shell);
		bcopy((char *)&pwd.pw_expire, p, sizeof(time_t));
		p += sizeof(time_t);
		data.dsize = p - buf;

		/* Store insecure by name. */
		tbuf[0] = _PW_KEYBYNAME;
		len = strlen(pwd.pw_name);
		bcopy(pwd.pw_name, tbuf + 1, len);
		key.dsize = len + 1;
		if (dbm_store(dp, key, data, DBM_INSERT) < 0)
			error("dbm file");

		/* Store insecure by number. */
		tbuf[0] = _PW_KEYBYNUM;
		bcopy((char *)&cnt, tbuf + 1, sizeof(cnt));
		key.dsize = sizeof(cnt) + 1;
		if (dbm_store(dp, key, data, DBM_INSERT) < 0)
			error("dbm file");

		/* Store insecure by uid. */
		tbuf[0] = _PW_KEYBYUID;
		bcopy((char *)&pwd.pw_uid, tbuf + 1, sizeof(pwd.pw_uid));
		key.dsize = sizeof(pwd.pw_uid) + 1;
		if (dbm_store(dp, key, data, DBM_INSERT) < 0)
			error("dbm file");

		/* Create secure data. */
		p = buf;
		COMPACT(pwd.pw_name);
		COMPACT(pwd.pw_passwd);
		bcopy((char *)&pwd.pw_uid, p, sizeof(int));
		p += sizeof(int);
		bcopy((char *)&pwd.pw_gid, p, sizeof(int));
		p += sizeof(int);
		bcopy((char *)&pwd.pw_change, p, sizeof(time_t));
		p += sizeof(time_t);
		COMPACT(pwd.pw_class);
		COMPACT(pwd.pw_gecos);
		COMPACT(pwd.pw_dir);
		COMPACT(pwd.pw_shell);
		bcopy((char *)&pwd.pw_expire, p, sizeof(time_t));
		p += sizeof(time_t);
		data.dsize = p - buf;

		/* Store secure by name. */
		tbuf[0] = _PW_KEYBYNAME;
		len = strlen(pwd.pw_name);
		bcopy(pwd.pw_name, tbuf + 1, len);
		key.dsize = len + 1;
		if (dbm_store(edp, key, data, DBM_INSERT) < 0)
			error("dbm file");

		/* Store secure by number. */
		tbuf[0] = _PW_KEYBYNUM;
		bcopy((char *)&cnt, tbuf + 1, sizeof(cnt));
		key.dsize = sizeof(cnt) + 1;
		if (dbm_store(edp, key, data, DBM_INSERT) < 0)
			error("dbm file");

		/* Store secure by uid. */
		tbuf[0] = _PW_KEYBYUID;
		bcopy((char *)&pwd.pw_uid, tbuf + 1, sizeof(pwd.pw_uid));
		key.dsize = sizeof(pwd.pw_uid) + 1;
		if (dbm_store(edp, key, data, DBM_INSERT) < 0)
			error("dbm file");

		/* Create original format password file entry */
		if (makeold)
			(void)fprintf(oldfp, "%s:*:%d:%d:%s:%s:%s\n",
			    pwd.pw_name, pwd.pw_uid, pwd.pw_gid, pwd.pw_gecos,
			    pwd.pw_dir, pwd.pw_shell);
	}
	(void)dbm_close(edp);
	(void)dbm_close(dp);
	if (makeold)
		(void)fclose(oldfp);

	/* Set master.passwd permissions, in case caller forgot. */
	(void)fchmod(fp, S_IRUSR|S_IWUSR);
	(void)fclose(fp);

	/* Install as the real password files. */
	(void)sprintf(buf, "%s.tmp.%s", _PATH_MP_DB, DBM_SUFFIX);
	mv(buf, _PATH_MP_DB);
	(void)sprintf(buf, "%s.tmp.%s", _PATH_SMP_DB, DBM_SUFFIX);
	mv(buf, _PATH_SMP_DB);
	if (makeold) {
		(void)sprintf(buf, "%s.orig", pname);
		mv(buf, _PATH_PASSWD);
	}
	/*
	 * Move the master password LAST -- chpass(1), passwd(1) and vipw(8)
	 * all use flock(2) on it to block other incarnations of themselves.
	 * The rename means that everything is unlocked, as the original file
	 * can no longer be accessed.
	 */
	mv(pname, _PATH_MASTERPASSWD);
	exit(0);
}

scan(fp, pw)
	FILE *fp;
	struct passwd *pw;
{
	static int lcnt;
	static char line[LINE_MAX];
	char *p;

	if (!fgets(line, sizeof(line), fp))
		return(0);
	++lcnt;
	/*
	 * ``... if I swallow anything evil, put your fingers down my
	 * throat...''
	 *	-- The Who
	 */
	if (!(p = index(line, '\n'))) {
		(void)fprintf(stderr, "pwd_mkdb: line too long\n");
		goto fmt;

	}
	*p = '\0';
	if (!pw_scan(line, pw)) {
		(void)fprintf(stderr, "pwd_mkdb: at line #%d.\n", lcnt);
fmt:		errno = EFTYPE;
		error(pname);
		exit(1);
	}
}

mv(from, to)
	char *from, *to;
{
	int sverrno;
	char buf[MAXPATHLEN];

	if (rename(from, to)) {
		sverrno = errno;
		(void)sprintf(buf, "%s to %s", from, to);
		errno = sverrno;
		error(buf);
	}
}

cleanup()
{
	char buf[MAXPATHLEN];

	switch(clean) {
	case FILE_ORIG:
		(void)sprintf(buf, "%s.orig", pname);
		(void)unlink(buf);
		/* FALLTHROUGH */
	case FILE_SECURE:
		(void)sprintf(buf, "%s.tmp", _PATH_SMP_DB);
		(void)unlink(buf);
		/* FALLTHROUGH */
	case FILE_INSECURE:
		(void)sprintf(buf, "%s.tmp", _PATH_MP_DB);
		(void)unlink(buf);
	}
}

error(name)
	char *name;
{
	(void)fprintf(stderr, "pwd_mkdb: %s: %s\n", name, strerror(errno));
	cleanup();
	exit(1);
}

usage()
{
	(void)fprintf(stderr, "usage: pwd_mkdb [-p] file\n");
	exit(1);
}
