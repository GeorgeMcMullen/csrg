/*
 * Copyright (c) 1982, 1986, 1989 Regents of the University of California.
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
 *	@(#)file.h	7.6 (Berkeley) 06/28/90
 */

#ifdef KERNEL
#include "fcntl.h"
#include "unistd.h"

/*
 * Descriptor table entry.
 * One for each kernel object.
 */
struct file {
	int	f_flag;		/* see below */
#define	DTYPE_VNODE	1	/* file */
#define	DTYPE_SOCKET	2	/* communications endpoint */
	short	f_type;		/* descriptor type */
	short	f_count;	/* reference count */
	short	f_msgcount;	/* references from message queue */
	struct	ucred *f_cred;	/* credentials associated with descriptor */
	struct	fileops {
		int	(*fo_read)();
		int	(*fo_write)();
		int	(*fo_ioctl)();
		int	(*fo_select)();
		int	(*fo_close)();
	} *f_ops;
	caddr_t	f_data;		/* inode */
	off_t	f_offset;
};

struct file *file, *fileNFILE;
int nfile;

/* convert O_RDONLY/O_WRONLY/O_RDWR to FREAD/FWRITE */
#define	FOPEN		(-1)
#define	FREAD		1
#define	FWRITE		2

/* kernel only versions -- deprecated, should be removed */
#define	FCREAT		O_CREAT
#define	FDEFER		O_DEFER
#define	FEXCL		O_EXCL
#define	FEXLOCK		O_EXLOCK
#define	FMARK		O_MARK
#define	FSHLOCK		O_SHLOCK
#define	FTRUNC		O_TRUNC

/* bits to save after open */
#define	FMASK		(FREAD|FWRITE|O_APPEND|O_ASYNC|O_NONBLOCK)
/* bits not settable by fcntl(F_SETFL, ...) */
#define	FCNTLCANT	(FREAD|FWRITE|O_DEFER|O_EXLOCK|O_MARK|O_SHLOCK)

#else

#include <sys/fcntl.h>
#include <sys/unistd.h>

#endif

/* operation for lseek(2); renamed by POSIX 1003.1 to unistd.h */
#define	L_SET		0	/* set file offset to offset */
#define	L_INCR		1	/* set file offset to current plus offset */
#define	L_XTND		2	/* set file offset to EOF plus offset */
