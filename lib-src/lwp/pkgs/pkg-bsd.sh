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
    mkdir devel
    MakeSubTree ${ver} ${bsd}

#    chown -R root.root devel
}

function MakeSubTree () {
    ver=$1
    bsd=$2

    mkdir devel/lwp
    MakeMakefile ${ver} devel/lwp/Makefile ${bsd}

    mkdir devel/lwp/files

    if [ x${bsd} = xNetBSD ]
    then
	cat > /tmp/mf << EOF
\$NetBSD\$

EOF
    else
	cp /dev/null /tmp/mf
    fi

    for dir in . .. ../.. ; do
      if [ -f ${dir}/lwp-${ver}.tar.gz ] ; then
	( cd ${dir} ; md5sum lwp-${ver}.tar.gz | awk '{printf("MD5 (%s) = %s\n",$2,$1)}' >> /tmp/mf )
	mv /tmp/mf devel/lwp/files/md5
	break;
      fi
    done
    rm -f /tmp/mf

    mkdir devel/lwp/pkg

    cat > devel/lwp/pkg/COMMENT << EOF
LWP thread library
EOF
    cat > devel/lwp/pkg/DESCR << EOF
The LWP userspace threads library. The LWP threads library is used by the Coda
distributed filesystem, RVM (a persistent VM library), and RPC2/SFTP (remote
procedure call library)
EOF

    MakePLIST devel/lwp/pkg/PLIST ${bsd}
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
CATEGORIES=	devel
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
USE_LIBTOOL=	yes

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
lib/liblwp.so.1.0
EOF
    else
	cat > ${dst} << EOF
lib/liblwp.so
lib/liblwp.so.1
EOF
    fi

    cat >> ${dst} << EOF
lib/liblwp.a
lib/liblwp.la
include/lwp/lock.h
include/lwp/lwp.h
include/lwp/preempt.h
include/lwp/timer.h
EOF
}

MakePortsTree $VERSION $BSD
tar -czf pkg-lwp-$VERSION-$BSD.tgz devel
rm -rf devel

