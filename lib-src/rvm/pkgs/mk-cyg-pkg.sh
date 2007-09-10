# Make a cygwin package.
#

CYGWINREV=1
PKG=rvm
FLIST="usr/include/rvm/rds.h usr/include/rvm/rvm.h \
       usr/include/rvm/rvm_segment.h usr/include/rvm/rvm_statistics.h \
       usr/lib/librds.a usr/lib/librds.la usr/lib/librdslwp.a \
       usr/lib/librdslwp.la usr/lib/librvm.a usr/lib/librvm.la \
       usr/lib/librvmlwp.a usr/lib/librvmlwp.la \
       usr/lib/libseg.a usr/lib/libseg.la \
       usr/sbin/rdsinit.exe usr/sbin/rvmutl.exe" 
       

# Sanity checks ...

if [ `uname -o` != Cygwin ] ; then
    echo This script must be run under Cygwin
    exit 1
fi

WD=`pwd`

if [ `basename $WD` != $PKG ] ; then
   if [ basename `dirname $WD` != $PKG ] ; then
       echo This script must be started in $PKG or $PKG/pkgs
       exit 1
   fi
   cd ..
   WD=`pwd`
   if [ `basename $WD` != $PKG ] ; then
       echo This script must be started in $PKG or $PKG/pkgs
       exit 1
   fi
fi

# Get the revision number ...
function AM_INIT_AUTOMAKE() { \
  REV=$2; \
}
eval `grep INIT_AUTOMAKE configure.in | tr "(,)" "   "`
if [ x$REV = x ] ; then
    echo Could not get revision number
    exit 1
fi

echo Building $PKG-$REV cygwin binary and source packages

# Bootstrap it !

echo Running bootstrap.sh
if ! ./bootstrap.sh ; then
  echo "Can't bootstrap.  Stoppped."
  exit 1
fi

# Build it ..

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

if ! ../configure --prefix=/usr ; then
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

# strip it!

echo Stripping files.

for f in $FLIST ; do 
  if [ `basename $f` != `basename $f .exe` ] ; then
      echo Stripping /$f
      strip /$f
  fi
done

# package it

echo "Creating binary tar.bz2 file."

(cd / ; if ! tar -cjf $WD/$PKG-$REV-$CYGWINREV.tar.bz2 $FLIST ; then \
    echo Could not create the tar file. ; \
    rm $WD/$PKG-$REV-$CYGWINREV.tar.bz2 ; \
fi )
if [ ! -f $PKG-$REV-$CYGWINREV.tar.bz2 ] ; then
    exit 1;
fi

# make source tar

SRCLST=`grep / CVS/Entries  | grep -v .cvsignore | cut -d/ -f2`

echo "Creating source tar.bz2 file."

if ! mkdir $PKG-$REV-$CYGWINREV ; then
    echo Could not make new source dir.
    exit 1
fi

if ! cp -rp $SRCLST $PKG-$REV-$CYGWINREV ; then
    echo Could not copy sources.
    exit 1
fi

find $PKG-$REV-$CYGWINREV -name CVS -exec rm -rf \{\} \;

tar -cjf $PKG-$REV-$CYGWINREV-src.tar.bz22 $PKG-$REV-$CYGWINREV

# cleanup

echo Cleaning ...

rm -rf $PKG-$REV-$CYGWINREV zobj-cygpkg
