
dnl ---------------------------------------------
dnl translate easy to remember target names into recognizable gnu variants and
dnl test the cross compilation platform and adjust default settings

AC_DEFUN([CODA_SETUP_BUILD],
[AC_SUBST(LIBTOOL_LDFLAGS)
case ${host_alias} in
  djgpp | win95 | dos )  host_alias=i386-pc-msdos ;;
  cygwin* | winnt | nt ) host_alias=i386-pc-cygwin ;;
  arm ) host_alias=arm-unknown-linux-gnuelf ;;
esac
AC_CANONICAL_HOST
if test ${cross_compiling} = yes ; then
  dnl We have to override some things the configure script tends to
  dnl get wrong as it tests the build platform feature
  ac_cv_func_mmap_fixed_mapped=yes

  case ${host} in
   i386-pc-msdos )
    dnl shared libraries don't work here
    enable_shared=no
    CC="dos-gcc -bmmap"
    CXX="dos-gcc -bmmap"
    AR="dos-ar"
    RANLIB="true"
    AS="dos-as"
    NM="dos-nm"
    ;;
   i386-pc-cygwin )
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

    dnl We seem to need these to get a dll built
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
fi])

dnl ---------------------------------------------
dnl Define library version

AC_SUBST(LIBTOOL_VERSION)
AC_SUBST(MAJOR_VERSION)
AC_SUBST(LINUX_VERSION)
AC_SUBST(DLL_VERSION)
AC_SUBST(FREEBSD_VERSION)
AC_SUBST(GENERIC_VERSION)
AC_DEFUN([CODA_LIBRARY_VERSION],
  [LIBTOOL_VERSION="$2:$1:$3"; major=`expr $2 - $3`
   MAJOR_VERSION="$major"
   LINUX_VERSION="$major.$3.$1"
   DLL_VERSION="$major-$3-$1"
   FREEBSD_VERSION="$2"
   GENERIC_VERSION="$2.$1"])

