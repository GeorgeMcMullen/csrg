#	@(#)Makefile	8.1 (Berkeley) 06/05/93

MAN7=	ascii.0 environ.0 hier.0 hostname.0 intro.0 mailaddr.0 \
	mdoc.samples.0 operator.0 term.0
MLINKS=	intro.7 miscellaneous.7

all: ${MAN7}

clean depend lint tags:

cleandir:
	rm -f ${MAN7}

install: maninstall

.include <bsd.prog.mk>
