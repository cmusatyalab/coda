#!/bin/sh

if [ $# != 2 ]
then
  echo "Usage $0 <version> [NetBSD|FreeBSD]"
  exit
fi

VERSION=$1
BSD=$2

function MakePortsTree () {
    ver=$1
    bsd=$2

    # make ports stuff
    mkdir lib
    MakeSubTree ${ver} ${bsd}

#    chown -R root.root lib
}

function MakeSubTree () {
    ver=$1
    bsd=$2

    mkdir lib/lwp
    MakeMakefile ${ver} lib/lwp/Makefile ${bsd}

    mkdir lib/lwp/files

    if [ x${bsd} = xNetBSD ]
    then
	cat > /tmp/mf << EOF
\$NetBSD\$

EOF
    else
	cp /dev/null /tmp/mf
    fi

    if [ -f lwp-${ver}.tar.gz ] ; then
	md5sum lwp-${ver}.tar.gz >> /tmp/mf
	mv /tmp/mf lib/lwp/files/md5
    elif [ -f ../lwp-${ver}.tar.gz ] ; then
	md5sum ../lwp-${ver}.tar.gz >> /tmp/mf
	mv /tmp/mf lib/lwp/files/md5
    fi
    rm -f /tmp/mf

    mkdir lib/lwp/pkg

    cat > lib/lwp/pkg/COMMENT << EOF
LWP thread library
EOF
    cat > lib/lwp/pkg/DESCR << EOF
The LWP userspace threads library. The LWP threads library is used by the Coda
distributed filesystem, RVM (a persistent VM library), and RPC2/SFTP (remote
procedure call library)
EOF

    MakePLIST lib/lwp/pkg/PLIST ${bsd}
}

function MakeMakefile () {
  version=$1
  dest=$2
  if [ x$3 = xNetBSD ]
  then
    REMOVE=FreeBSD
    KEEP=NetBSD
  else
    REMOVE=NetBSD
    KEEP=FreeBSD
  fi

  cat > /tmp/mf << EOF
@NetBSD # \$NetBSD\$
@NetBSD #
@FreeBSD # New ports collection makefile for:	lwp
@FreeBSD # Version required:			@VERSION@
@FreeBSD # Date created:				@DATE@
@FreeBSD # Whom:					@USER@
@FreeBSD # \$FreeBSD\$
@FreeBSD #

DISTNAME=	lwp-@VERSION@
PKGNAME=	lwp-@VERSION@
CATEGORIES=	lib
MASTER_SITES=	ftp://ftp.coda.cs.cmu.edu/pub/coda/src/
EXTRACT_SUFX=	.tar.gz

MAINTAINER=	coda@cs.cmu.edu
@NetBSD HOMEPAGE=	http://www.coda.cs.cmu.edu/

@NetBSD ONLY_FOR_ARCHS=	arm32 i386 ns32k
@NetBSD 
@NetBSD LICENSE=	LGPL
@NetBSD 
ALL_TARGET=	all
INSTALL_TARGET=	install

GNU_CONFIGURE=	yes
USE_GMAKE=	yes

@NetBSD .include "../../mk/bsd.pkg.mk"
@FreeBSD .include <bsd.port.mk>
EOF

    cat /tmp/mf | sed -e "s/@VERSION@/${version}/" | \
		  sed -e "s/@DATE@/`date`/" | \
		  sed -e "s/@USER@/${USER}/" | \
		  sed -e "/^@${REMOVE} .*$/d" | \
		  sed -e "s/^@${KEEP} \(.*\)$/\1/" > ${dest}
    rm /tmp/mf
}

function MakePLIST () {
    dst=$1
    bsd=$2

    if [ x${bsd} = xNetBSD ]
    then
	cat > ${dst} << EOF
@comment \$NetBSD\$
EOF
    else
	cp /dev/null ${dst}
    fi

    cat >> ${dst} << EOF
lib/liblwp.so*
include/lwp/*
@dirrm lib/coda
EOF
}

MakePortsTree $VERSION $BSD
tar -czf pkg-lwp-$VERSION-$BSD.tgz lib
rm -rf lib

