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
PKGNAME=	coda-@PKG@-@VERSION@
CATEGORIES=	net
MASTER_SITES=	ftp://ftp.coda.cs.cmu.edu/pub/coda/src/
EXTRACT_SUFX=	.tgz

MAINTAINER=	rvb@cs.cmu.edu
@NetBSD HOMEPAGE=	http://www.coda.cs.cmu.edu/
@FreeBSD LIB_DEPENDS+=	lwp.1:\${PORTSDIR}/devel/lwp
@FreeBSD LIB_DEPENDS+=	rpc2.2:\${PORTSDIR}/devel/rpc2
@FreeBSD LIB_DEPENDS+=	rvm.1:\${PORTSDIR}/devel/rvm

@NetBSD DEPENDS+=	readline-2.2:../../devel/readline
@NetBSD DEPENDS+=	perl-5.00404:../../lang/perl5
@NetBSD DEPENDS+=	lwp-1.4:../../devel/lwp
@NetBSD DEPENDS+=	rpc2-1.5:../../devel/rpc2
@NetBSD DEPENDS+=	rvm-1.2:../../devel/rvm

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
lib/coda/Advice.tcl
lib/coda/CodaConsole
lib/coda/Consider.tcl
lib/coda/ConsiderAdding.tcl
lib/coda/ConsiderRemoving.tcl
lib/coda/ControlPanel.tcl
lib/coda/Date.tcl
lib/coda/DiscoMiss.tcl
lib/coda/Events.tcl
lib/coda/Globals.tcl
lib/coda/Helper.tcl
lib/coda/HoardWalk.tcl
lib/coda/HoardWalkAdvice.tcl
lib/coda/Indicators.tcl
lib/coda/Initialization.tcl
lib/coda/Lock.tcl
lib/coda/Log.tcl
lib/coda/Network.tcl
lib/coda/OutsideWorld.tcl
lib/coda/ReadMiss.tcl
lib/coda/Reconnection.tcl
lib/coda/Reintegration.tcl
lib/coda/Repair.tcl
lib/coda/Space.tcl
lib/coda/Task.tcl
lib/coda/Timing.tcl
lib/coda/Tokens.tcl
lib/coda/WeakMiss.tcl
lib/coda/tixCodaMeter.tcl
sbin/au
sbin/venus
sbin/venus-setup
sbin/volmunge
sbin/vutil
bin/advice_srv
bin/cfs
bin/clog
bin/cmon
bin/codacon
bin/cpasswd
bin/ctokens
bin/cunlog
bin/filerepair
bin/hoard
bin/logbandwidth
bin/logcmls
bin/logprogress
bin/logreintegration
bin/parser
bin/removeinc
bin/repair
bin/replay
bin/spy
bin/xaskuser
bin/xfrepair
etc/coda/venus.conf.ex
@dirrm lib/coda
EOF
    fi

    if [ x${pkg} = xserver ]
    then
	cat >> ${dst} << EOF
bin/norton
bin/norton-reinit
bin/reinit
sbin/auth2
sbin/backup
sbin/backup.sh
sbin/bldvldb.sh
sbin/codaconfedit
sbin/codasrv
sbin/createvol_rep
sbin/initpw
sbin/inoder
sbin/makeftree
sbin/merge
sbin/parserecdump
sbin/partial-reinit.sh
sbin/pdbtool
sbin/printvrdb
sbin/purgevol
sbin/purgevol_rep
sbin/pwdtopdbtool.py
sbin/readdump
sbin/rpc2portmap
sbin/startserver
sbin/tape.pl
sbin/updateclnt
sbin/updatefetch
sbin/updatesrv
sbin/vice-killvolumes
sbin/vice-setup
sbin/vice-setup-ports
sbin/vice-setup-rvm
sbin/vice-setup-scm
sbin/vice-setup-srvdir
sbin/vice-setup-user
sbin/volutil
etc/rc.d/rc.vice
EOF
    fi
}

MakePortsTree $VERSION $BSD

