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
    mkdir net
    MakeSubTree client ${ver} ${bsd}
    MakeSubTree server ${ver} ${bsd}

#    chown -R root.root net
}

function MakeSubTree () {
    pkg=$1
    ver=$2
    bsd=$3

    mkdir net/coda5_${pkg}
    MakeMakefile ${pkg} ${ver} net/coda5_${pkg}/Makefile ${bsd}

    mkdir net/coda5_${pkg}/files

    if [ x${bsd} = xNetBSD ]
    then
	cat > /tmp/mf << EOF
\$NetBSD\$

EOF
    else
	cp /dev/null /tmp/mf
    fi

    if [ -f /coda/project/releases/${ver}/src/coda-${ver}.md5 ] ; then
	cat /coda/project/releases/${ver}/src/coda-${ver}.md5 >> /tmp/mf
	mv /tmp/mf net/coda5_${pkg}/files/md5
    fi
    rm -f /tmp/mf

    mkdir net/coda5_${pkg}/pkg
    MakeCOMMENT ${pkg} net/coda5_${pkg}/pkg/COMMENT
    MakeDESCR ${pkg} net/coda5_${pkg}/pkg/DESCR
    MakePLIST ${pkg} net/coda5_${pkg}/pkg/PLIST ${bsd}
}

function MakeMakefile () {
  package=$1
  version=$2
  dest=$3
  if [ x$4 = xNetBSD ]
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
@FreeBSD # New ports collection makefile for:	coda-@PKG@
@FreeBSD # Version required:			@VERSION@
@FreeBSD # Date created:				@DATE@
@FreeBSD # Whom:					@USER@#
@FreeBSD # \$FreeBSD\$
@FreeBSD #

DISTNAME=	coda-@VERSION@
@NetBSD PKGNAME=	coda-@PKG@-@VERSION@
@FreeBSD PORTNAME=	coda-@PKG@
@FreeBSD PORTVERSION=	@VERSION@
CATEGORIES=	net
MASTER_SITES=	ftp://ftp.coda.cs.cmu.edu/pub/coda/src/
EXTRACT_SUFX=	.tar.gz

MAINTAINER=	coda@cs.cmu.edu
@NetBSD HOMEPAGE=	http://www.coda.cs.cmu.edu/
@FreeBSD LIB_DEPENDS+=	lwp.2:\${PORTSDIR}/devel/lwp
@FreeBSD LIB_DEPENDS+=	rpc2.4:\${PORTSDIR}/devel/rpc2
@FreeBSD LIB_DEPENDS+=	rvm.2:\${PORTSDIR}/devel/rvm

@NetBSD DEPENDS+=	readline-2.2:../../devel/readline
@NetBSD DEPENDS+=	perl-5.00404:../../lang/perl5
@NetBSD DEPENDS+=	lwp-1.6:../../devel/lwp
@NetBSD DEPENDS+=	rpc2-1.9:../../devel/rpc2
@NetBSD DEPENDS+=	rvm-1.3:../../devel/rvm

@NetBSD #ONLY_FOR_ARCHS=	arm32 i386 ns32k
@NetBSD 
@NetBSD LICENSE=	GPL
@NetBSD 
ALL_TARGET=	coda
INSTALL_TARGET=	@PKG@-install

@NetBSD USE_PERL5=	yes
GNU_CONFIGURE=	yes
USE_GMAKE=	yes

@NetBSD .include "../../mk/bsd.pkg.mk"
@FreeBSD .include <bsd.port.mk>
EOF

    cat /tmp/mf | sed -e "s/@PKG@/${package}/" | \
		  sed -e "s/@VERSION@/${version}/" | \
		  sed -e "s/@DATE@/`date`/" | \
		  sed -e "s/@USER@/${USER}/" | \
		  sed -e "/^@${REMOVE} .*$/d" | \
		  sed -e "s/^@${KEEP} \(.*\)$/\1/" > ${dest}
    rm /tmp/mf
}

function MakeCOMMENT () {
    pkg=$1
    dst=$2

    cat > /tmp/text << EOF
@PKG@ programs for a replicated high-performance network file system
EOF

    sed -e "s/@PKG@/${pkg}/" < /tmp/text > ${dst}
    rm /tmp/text
}

function MakeDESCR () {
    pkg=$1
    dst=$2

    cat > /tmp/text << EOF
Coda is a distributed file system.  Among its features are disconnected
operation, good security model, server replication and persistent client
side caching. 

This package builds the entire source tree but only installs(/packages) the
@PKG@ side programs.

For more info, contact <coda@cs.cmu.edu> or visit http://www.coda.cs.cmu.edu.
EOF

    sed -e "s/@PKG@/${pkg}/" < /tmp/text > ${dst}
    rm /tmp/text
}

function MakePLIST () {
    pkg=$1
    dst=$2
    bsd=$3

    if [ x${bsd} = xNetBSD ]
    then
	cat > ${dst} << EOF
@comment \$NetBSD\$
EOF
    else
	cp /dev/null ${dst}
    fi

    if [ x${pkg} = xclient ]
    then
	cat >> ${dst} << EOF
sbin/codaconfedit
sbin/venus
sbin/venus-setup
sbin/volmunge
sbin/vutil
bin/au
bin/cfs
bin/clog
bin/cmon
bin/codacon
bin/cpasswd
bin/ctokens
bin/cunlog
bin/filerepair
bin/hoard
bin/mkcodabf
bin/mklka
bin/parser
bin/removeinc
bin/repair
bin/coda_replay
bin/spy
bin/xaskuser
bin/xfrepair
man/man1/au.1
man/man1/cfs.1
man/man1/clog.1
man/man1/cmon.1
man/man1/coda_replay.1
man/man1/cpasswd.1
man/man1/ctokens.1
man/man1/cunlog.1
man/man1/hoard.1
man/man1/mkcodabf.1
man/man1/repair.1
man/man1/spy.1
man/man8/venus.8
man/man8/venus-setup.8
man/man8/volmunge.8
man/man8/vutil.8
etc/coda/realms
etc/coda/venus.conf.ex
EOF
    fi

    if [ x${pkg} = xserver ]
    then
	cat >> ${dst} << EOF
bin/getvolinfo
bin/norton
bin/norton-reinit
bin/reinit
bin/rpc2ping
bin/smon2
sbin/auth2
sbin/backup
sbin/backup.sh
sbin/bldvldb.sh
sbin/codaconfedit
sbin/codadump2tar
sbin/codasrv
sbin/createvol_rep
sbin/initpw
sbin/inoder
sbin/merge
sbin/parserecdump
sbin/partial-reinit.sh
sbin/pdbtool
sbin/printvrdb
sbin/purgevol_rep
sbin/readdump
sbin/startserver
sbin/tape.pl
sbin/updateclnt
sbin/updatefetch
sbin/updatesrv
sbin/vice-killvolumes
sbin/vice-setup
sbin/coda-server-logrotate
sbin/vice-setup-rvm
sbin/vice-setup-scm
sbin/vice-setup-srvdir
sbin/vice-setup-user
sbin/volutil
man/man5/backuplogs.5
man/man5/dumpfile.5
man/man5/dumplist.5
man/man5/passwd.coda.5
man/man5/maxgroupid.5
man/man5/vicetab.5
man/man5/volumelist.5
man/man5/vrdb.5
man/man8/auth2.8
man/man8/backup.8
man/man8/bldvldb.sh.8
man/man8/createvol_rep.8
man/man8/initpw.8
man/man8/merge.8
man/man8/pdbtool.8
man/man8/purgevol_rep.8
man/man8/readdump.8
man/man8/startserver.8
man/man8/updateclnt.8
man/man8/updatesrv.8
man/man8/vice-setup.8
man/man8/volutil.8
etc/coda/server.conf.ex
etc/rc.d/rc.vice
EOF
    fi
}

MakePortsTree $VERSION $BSD

