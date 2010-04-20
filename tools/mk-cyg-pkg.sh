# Make a cygwin package.
#

DOCLEAN=true

if [ x$NOBUILD = x ] ; then
  BUILD=true
else
  BUILD=false
fi

CYGWINREV=2
PKG=coda
CLILIST="
    etc/coda/realms
    etc/coda/venus.conf.ex
    etc/postinstall/coda-client.sh
    usr/bin/au.exe
    usr/bin/cfs.exe
    usr/bin/clog.exe
    usr/bin/cmon.exe
    usr/bin/coda_replay.exe
    usr/bin/codacon.exe
    usr/bin/cpasswd.exe
    usr/bin/ctokens.exe
    usr/bin/cunlog.exe
    usr/bin/filerepair.exe
    usr/bin/gcodacon
    usr/bin/hoard.exe
    usr/bin/mkcodabf.exe
    usr/bin/mklka.exe
    usr/bin/parser.exe
    usr/bin/removeinc.exe
    usr/bin/repair.exe
    usr/bin/spy.exe
    usr/bin/vcodacon.exe
    usr/bin/xaskuser
    usr/bin/xfrepair
    usr/sbin/asrlauncher.exe
    usr/sbin/codaconfedit.exe
    usr/sbin/venus-setup
    usr/sbin/venus.exe
    usr/sbin/volmunge
    usr/sbin/vutil.exe
    usr/share/man/man1/au.1
    usr/share/man/man1/cfs.1
    usr/share/man/man1/clog.1
    usr/share/man/man1/cmon.1
    usr/share/man/man1/coda_replay.1
    usr/share/man/man1/cpasswd.1
    usr/share/man/man1/ctokens.1
    usr/share/man/man1/cunlog.1
    usr/share/man/man1/hoard.1
    usr/share/man/man1/mkcodabf.1
    usr/share/man/man1/repair.1
    usr/share/man/man1/spy.1
    usr/share/man/man8/venus-setup.8
    usr/share/man/man8/venus.8
    usr/share/man/man8/volmunge.8
    usr/share/man/man8/vutil.8 
 "

SRVLIST="
    etc/coda/server.conf.ex
    usr/bin/getvolinfo.exe
    usr/bin/reinit
    usr/bin/rpc2ping.exe
    usr/bin/rvmsizer.exe
    usr/bin/smon2.exe
    usr/sbin/auth2.exe
    usr/sbin/backup.exe
    usr/sbin/backup.sh
    usr/sbin/bldvldb.sh
    usr/sbin/coda-server-logrotate
    usr/sbin/codaconfedit.exe
    usr/sbin/codadump2tar.exe
    usr/sbin/codasrv.exe
    usr/sbin/codastart
    usr/sbin/createvol_rep
    usr/sbin/initpw.exe
    usr/sbin/inoder.exe
    usr/sbin/merge.exe
    usr/sbin/norton-reinit.exe
    usr/sbin/norton.exe
    usr/sbin/parserecdump.exe
    usr/sbin/partial-reinit.sh
    usr/sbin/pdbtool.exe
    usr/sbin/printvrdb.exe
    usr/sbin/purgevol_rep
    usr/sbin/readdump.exe
    usr/sbin/startserver
    usr/sbin/tape.pl
    usr/sbin/tokentool.exe
    usr/sbin/updateclnt.exe
    usr/sbin/updatefetch.exe
    usr/sbin/updatesrv.exe
    usr/sbin/vice-killvolumes
    usr/sbin/vice-setup
    usr/sbin/vice-setup-rvm
    usr/sbin/vice-setup-scm
    usr/sbin/vice-setup-srvdir
    usr/sbin/vice-setup-user
    usr/sbin/volutil.exe
    usr/share/man/man5/backuplogs.5
    usr/share/man/man5/dumpfile.5
    usr/share/man/man5/dumplist.5
    usr/share/man/man5/maxgroupid.5
    usr/share/man/man5/passwd.coda.5
    usr/share/man/man5/servers.5
    usr/share/man/man5/vicetab.5
    usr/share/man/man5/volumelist.5
    usr/share/man/man5/vrdb.5
    usr/share/man/man8/auth2.8
    usr/share/man/man8/backup.8
    usr/share/man/man8/bldvldb.sh.8
    usr/share/man/man8/codasrv.8
    usr/share/man/man8/createvol_rep.8
    usr/share/man/man8/initpw.8
    usr/share/man/man8/merge.8
    usr/share/man/man8/norton.8
    usr/share/man/man8/pdbtool.8
    usr/share/man/man8/purgevol_rep.8
    usr/share/man/man8/readdump.8
    usr/share/man/man8/startserver.8
    usr/share/man/man8/updateclnt.8
    usr/share/man/man8/updatesrv.8
    usr/share/man/man8/vice-setup.8
    usr/share/man/man8/volutil.8
"

# etc/rc.vice

# Sanity checks ...

if [ `uname -o` != Cygwin ] ; then
    echo This script must be run under Cygwin
    exit 1
fi

WD=`pwd`

if [ `basename $WD` != $PKG ] ; then
   DIR=`dirname $WD`
   if [ `basename $DIR` != $PKG ] ; then
       echo This script must be started in $PKG or $PKG/tools
       exit 1
   fi
   cd ..
   WD=`pwd`
   if [ `basename $WD` != $PKG ] ; then
       echo This script must be started in $PKG or $PKG/tools
       exit 1
   fi
fi

# Get the revision number ...
function AC_INIT() { \
  REV=$2; \
}
eval `grep AC_INIT\( configure.ac | tr "(,)" "   "`
if [ x$REV = x ] ; then
    echo Could not get revision number
    exit 1
fi

echo Building $PKG-$REV cygwin binary and source packages

if $BUILD ; then 
    # Bootstrap it !
    
    echo Running bootstrap.sh
    if ! ./bootstrap.sh ; then
	echo "Can't bootstrap.  Stoppped."
	exit 1
    fi
    
    # Build it ..  First the client ...
    
    if [ ! -d zobj-cygpkg ] ; then 
	if ! mkdir zobj-cygpkg ; then
	    echo Could not make build directory.
	    exit 1
	fi
    fi
    
    if ! cd zobj-cygpkg ; then
	echo Could not cd to build directory.
	exit 1
    fi
    
    if ! ../configure --prefix=/usr --enable-client --with-vcodacon ; then
	echo Could not configure for build.
	exit 1
    fi
    
    if ! make ; then
	echo Could not make.
	exit 1;
    fi
    
    if ! make install ; then
	echo Could not install.
	exit 1;
    fi
    
    cd ..

    # strip the client
    
    echo Stripping client files.
    
    for f in $CLILIST ; do
	if [ `basename $f` != `basename $f .exe` ] ; then
	    echo Stripping /$f
	    strip /$f
	fi
    done
fi

# Add the postinstall script
cp tools/cyg-postinst.sh /etc/postinstall/coda-client.sh

# package the client

echo "Creating binary tar.bz2 file for the client."

(cd / ; if ! tar -cjf $WD/$PKG-client-$REV-$CYGWINREV.tar.bz2 $CLILIST ; then \
    echo Could not create the tar file. ; \
    rm $WD/$PKG-client-$REV-$CYGWINREV.tar.bz2 ; \
fi )
if [ ! -f $PKG-client-$REV-$CYGWINREV.tar.bz2 ] ; then
    exit 1;
fi

rm /etc/postinstall/coda-client.sh


if $BUILD ; then
    # Build the server 

    if ! cd zobj-cygpkg ; then
	echo Could not cd to zobj-cygpkg
	exit 1;
    fi
    
    echo "Building the Server"
    
    if ! ../configure --prefix=/usr --enable-server ; then
	echo Could not configure for build.
	exit 1
    fi
    
    if ! make ; then
	echo Could not make.
	exit 1;
    fi
    
    if ! make install ; then
	echo Could not install.
	exit 1;
    fi
    
    cd ..
    
    # strip the server

    echo Stripping server files.
    
    for f in $SRVLIST ; do 
	if [ `basename $f` != `basename $f .exe` ] ; then
	    echo Stripping /$f
	    strip /$f
	fi
    done
fi
    
# package the server

echo "Creating binary tar.bz2 file for the server."

(cd / ; if ! tar -cjf $WD/$PKG-server-$REV-$CYGWINREV.tar.bz2 $SRVLIST ; then \
    echo Could not create the tar file. ; \
    rm $WD/$PKG-server-$REV-$CYGWINREV.tar.bz2 ; \
fi )
if [ ! -f $PKG-server-$REV-$CYGWINREV.tar.bz2 ] ; then
    exit 1;
fi

# make source tar

SRCLST=`grep / CVS/Entries  | grep -v .cvsignore | cut -d/ -f2`

echo "Creating source tar.bz2 file."

if ! mkdir $PKG-server-$REV-$CYGWINREV ; then
    echo Could not make new source dir.
    exit 1
fi

if ! cp -rp $SRCLST $PKG-server-$REV-$CYGWINREV ; then
    echo Could not copy sources.
    exit 1
fi

find $PKG-server-$REV-$CYGWINREV -name CVS -exec rm -rf \{\} \;

tar -cjf $PKG-server-$REV-$CYGWINREV-src.tar.bz2 $PKG-server-$REV-$CYGWINREV

# cleanup
if $DOCLEAN ; then

echo Cleaning ...

rm -rf $PKG-server-$REV-$CYGWINREV zobj-cygpkg

fi
