
dnl ---------------------------------------------
dnl translate easy to remember target names into recognizable gnu variants and
dnl test the cross compilation platform and adjust default settings

AC_DEFUN(CODA_SETUP_BUILD,

[AC_SUBST(LIBTOOL_LDFLAGS)

case ${host_alias} in
  djgpp | dos ) host_alias=i386-pc-msdos ; dosmmap=false ;;
  win95 | win98 ) host_alias=i386-pc-msdos ; dosmmap=true ;;
  cygwin* | winnt | nt ) host_alias=i386-pc-cygwin ;;
  arm ) host_alias=arm-unknown-linux-gnuelf ;;
esac

AC_CANONICAL_HOST

dnl Shared libraries work for i386, mips, sparc, and alpha platforms. They
dnl might work for all platforms. If not, read the PORTING document.

dnl When cross-compiling, pick the right compiler toolchain.
dnl Can be overridden by providing the CROSS_COMPILE environment variable.
if test ${cross_compiling} = yes ; then
  dnl We have to override some things the configure script tends to
  dnl get wrong as it tests the build platform feature
  ac_cv_func_mmap_fixed_mapped=yes

  case ${host} in
   i*86-pc-msdos )
    dnl no shared libs for dos
    enable_shared=no

    if ${dosmmap} ; then 
      CC="dos-gcc -bmmap"	
      CXX="dos-gcc -bmmap"
    else
      CC="dos-gcc -bw95"	
      CXX="dos-gcc -bw95"
    fi

    AR="dos-ar"
    RANLIB="true"
    AS="dos-as"
    NM="dos-nm"

    dnl avoid using mmap for allocating lwp stacks
    ac_cv_func_mmap_fixed_mapped=no
    ;;
   i*86-pc-cygwin )
    dnl -D__CYGWIN32__ should be defined but sometimes isn't (wasn't?)
    CC="gnuwin32gcc -D__CYGWIN32__"
    CXX="gnuwin32g++"
    AR="gnuwin32ar"
    RANLIB="gnuwin32ranlib"
    AS="gnuwin32as"
    NM="gnuwin32nm"
    DLLTOOL="gnuwin32dlltool"
    OBJDUMP="gnuwin32objdump"

    LDFLAGS="-L/usr/gnuwin32/lib"

    dnl We need these to get a dll built
    libtool_flags="--enable-win32-dll"
    LIBTOOL_LDFLAGS="-no-undefined"
    ;;
   arm-unknown-linux-gnuelf )
    CROSS_COMPILE="arm-unknown-linuxelf-"
    ;;
 esac
fi
if test "${CROSS_COMPILE}" ; then
  CC=${CROSS_COMPILE}gcc
  CXX=${CROSS_COMPILE}g++
  CPP="${CC} -E"
  AS=${CROSS_COMPILE}as
  LD=${CROSS_COMPILE}ld
  AR=${CROSS_COMPILE}ar
  RANLIB=${CROSS_COMPILE}ranlib
  NM=${CROSS_COMPILE}nm
  OBJDUMP=${CROSS_COMPILE}objdump
  DLLTOOL=${CROSS_COMPILE}dlltool
  ac_cv_func_mmap_fixed_mapped=yes
fi
])

dnl ---------------------------------------------
dnl Define library version

AC_SUBST(LIBTOOL_VERSION)
AC_SUBST(LINUX_VERSION)
AC_SUBST(DLL_VERSION)
AC_SUBST(FREEBSD_VERSION)
AC_SUBST(GENERIC_VERSION)
AC_DEFUN(CODA_LIBRARY_VERSION,
  [LIBTOOL_VERSION="$2:$1:$3"; major=`expr $2 - $3`
   LINUX_VERSION="$major.$3.$1"
   DLL_VERSION="$major-$3-$1"
   FREEBSD_VERSION="$2"
   GENERIC_VERSION="$2.$1"])

dnl ---------------------------------------------
dnl Mac OS X has a weird compiler toolchain.
dnl
dnl Long story: The compiler tool chain in the Mac OS X Developer Tools is a
dnl strange beast. The compiler is based on the gcc 2.95.2 suite, with
dnl modifications to support the Objective C language and some Darwin quirks.
dnl The preprocessor (cpp) is available in two versions. One is the standard
dnl precompiler (from gcc 2.95.2), the other one is a special precompiler
dnl written by Apple, with support for precompiled headers. The latter one is
dnl used by default, because it is faster. However, some code doesn't compile
dnl with Apple's precompiler, so you must use the -traditional-cpp option to
dnl get the standard precompiler.
dnl
dnl (taken from http://fink.sourceforge.net/darwin/porting.php)

AC_DEFUN(CODA_DARWIN_BROKEN_CPP_WORKAROUND,
  [case "$build" in
    powerpc-apple-darwin*)
	CPPFLAGS="$CPPFLAGS -traditional-cpp"
	;;
   esac])

