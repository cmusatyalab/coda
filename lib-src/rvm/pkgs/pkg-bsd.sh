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

    mkdir devel/rvm
    MakeMakefile ${ver} devel/rvm/Makefile ${bsd}

    mkdir devel/rvm/files

    if [ x${bsd} = xNetBSD ]
    then
	cat > /tmp/mf << EOF
\$NetBSD\$

EOF
    else
	cp /dev/null /tmp/mf
    fi

    for dir in . .. ../.. ; do
      if [ -f ${dir}/rvm-${ver}.tar.gz ] ; then
	( cd ${dir} ; md5sum rvm-${ver}.tar.gz | awk '{printf("MD5 (%s) = %s\n",$2,$1)}' >> /tmp/mf )
	mv /tmp/mf devel/rvm/files/md5
	break;
      fi
    done
    rm -f /tmp/mf

    mkdir devel/rvm/pkg

    cat > devel/rvm/pkg/COMMENT << EOF
RVM persistent VM library
EOF
    cat > devel/rvm/pkg/DESCR << EOF
The RVM persistent VM library. The RVM library is used by the Coda distributed
filesystem.
EOF

    MakePLIST devel/rvm/pkg/PLIST ${bsd}
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
@FreeBSD # New ports collection makefile for:	rvm
@FreeBSD # Version required:			@VERSION@
@FreeBSD # Date created:				@DATE@
@FreeBSD # Whom:					@USER@
@FreeBSD # \$FreeBSD\$
@FreeBSD #

DISTNAME=	rvm-@VERSION@
PKGNAME=	rvm-@VERSION@
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
lib/librvm.so.1.0
lib/librvmlwp.so.1.0
lib/libseg.so.1.0
lib/librds.so.1.0
lib/librdslwp.so.1.0
EOF
    else
	cat > ${dst} << EOF
lib/librvm.so
lib/librvm.so.1
lib/librvmlwp.so
lib/librvmlwp.so.1
lib/libseg.so
lib/libseg.so.1
lib/librds.so
lib/librds.so.1
lib/librdslwp.so
lib/librdslwp.so.1
EOF
    fi

    cat >> ${dst} << EOF
sbin/rvmutl
sbin/rdsinit
lib/librvm.a
lib/librvm.la
lib/librvmlwp.a
lib/librvmlwp.la
lib/libseg.a
lib/libseg.la
lib/librds.a
lib/librds.la
lib/librdslwp.a
lib/librdslwp.la
include/rvm/rvm.h
include/rvm/rvm_statistics.h
include/rvm/rvm_segment.h
include/rvm/rds.h
EOF
}

MakePortsTree $VERSION $BSD
tar -czf pkg-rvm-$VERSION-$BSD.tgz devel
rm -rf devel

