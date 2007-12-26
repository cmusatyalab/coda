# Make a cygwin package.
#

if [ x$FULLMAKE = x ] ; then
    FULLMAKE=yes
fi

CYGWINREV=1
PKG=lwp
FLIST="usr/include/lwp/lock.h usr/include/lwp/lwp.h usr/include/lwp/timer.h \
       usr/lib/liblwp.a usr/lib/liblwp.la usr/lib/liblwp.dll.a \
       usr/lib/pkgconfig/lwp.pc usr/bin/cyglwp-2.dll"

# Sanity checks ...

if [ `uname -o` != Cygwin ] ; then
    echo This script must be run under Cygwin
    exit 1
fi

WD=`pwd`

if [ `basename $WD` != $PKG ] ; then
   DIR=`dirname $WD`
   if [ `basename $DIR` != $PKG ] ; then
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
function AC_INIT() { \
  REV=$2; \
}
eval `grep AC_INIT\( configure.in | tr "(,)" "   "`
if [ x$REV = x ] ; then
    echo Could not get revision number
    exit 1
fi

if [ $FULLMAKE = yes ] ; then
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
fi

# strip it!

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
    rm $WD/$PKG-$REV-$CYGWINREV.tar.bz ; \
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

tar -cjf $PKG-$REV-$CYGWINREV-src.tar.bz2 $PKG-$REV-$CYGWINREV

# cleanup

echo Cleaning ...

rm -rf $PKG-$REV-$CYGWINREV zobj-cygpkg
