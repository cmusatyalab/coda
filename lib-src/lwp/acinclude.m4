
dnl ---------------------------------------------
dnl translate easy to remember target names into recognizable gnu variants and
dnl test the cross compilation platform and adjust default settings

AC_DEFUN(CODA_SETUP_BUILD,
[case ${target} in
  djgpp | win95 | dos )   target=i386-pc-msdos ;;
  cygwin32 | winnt | nt ) target=i386-pc-cygwin32 ;;
esac
AC_CANONICAL_SYSTEM
if test ${build} != ${target} ; then
  case ${target} in
   i386-pc-msdos )
    dnl shared libraries don't work here
    AM_DISABLE_SHARED
    CC="dos-gcc -bmmap"
    CXX="dos-gcc -bmmap"
    AR="dos-ar"
    RANLIB="true"
    AS="dos-as"
    NM="dos-nm"

    dnl We have to override some things the configure script tends to
    dnl get wrong as it tests the build platform feature
    ac_cv_func_mmap_fixed_mapped=yes
    ;;
   i386-pc-cygwin32 )
    dnl shared libraries don't work here
    AM_DISABLE_SHARED
    dnl -D__CYGWIN32__ should be defined but sometimes isn't (wasn't?)
    CC="gnuwin32gcc -D__CYGWIN32__"
    CXX="gnuwin32g++"
    AR="gnuwin32ar"
    RANLIB="gnuwin32ranlib"
    AS="gnuwin32as"
    NM="gnuwin32nm"

    LDFLAGS="-L/usr/gnuwin32/lib"
    ;;
 esac
fi])

AC_DEFUN(CODA_PATCH_LIBTOOL,
[if test ${build} != ${target} ; then
  patch < ${srcdir}/libtool.patch
 fi])

AC_DEFUN(CODA_FUNC_INSQUE,
[AC_CHECK_FUNC(insque,,
  [AC_CHECK_LIB(iberty, insque,,
    [AC_CHECK_LIB(bfd, insque,,
      [AC_CHECK_LIB(compat, insque)
])])])])
