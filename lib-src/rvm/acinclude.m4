
dnl ---------------------------------------------
dnl translate easy to remember target names into recognizable gnu variants and
dnl test the cross compilation platform and adjust default settings

AC_DEFUN(CODA_SETUP_BUILD,
[AC_SUBST(LIBTOOL_LDFLAGS)
case ${target} in
  djgpp | win95 | dos )  target=i386-pc-msdos ;;
  cygwin* | winnt | nt ) target=i386-pc-cygwin ;;
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
   i386-pc-cygwin )
    dnl -D__CYGWIN32__ should be defined but sometimes isn't (wasn't?)
    host=i386-pc-cygwin
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
 esac
fi])

dnl ---------------------------------------------
dnl Specify paths to the lwp includes and libraries

AC_DEFUN(CODA_OPTION_LWP,
 [AC_SUBST(LWPINCLUDES)
  AC_ARG_WITH(lwp-includes,
    [  --with-lwp-includes     Location of the the lwp include files],
    [ CPPFLAGS="${CPPFLAGS} -I`(cd ${withval} ; pwd)`" ])
  AC_ARG_WITH(lwp-library,
    [  --with-lwp-library      Location of the lwp library files],
    [ LDFLAGS="${LDFLAGS} -L`(cd ${withval} ; pwd)`" ])
  AC_ARG_WITH(lwp-pt,
    [  --with-lwp-pt           Link against *experimental* lwp_pt library],
    [ with_LWP_PT=yes ; DEFS="${DEFS} -D_REENTRANT" ],
    [ with_LWP_PT=no ])
 ]) 

dnl ---------------------------------------------
dnl Search for an installed lwp library

AC_DEFUN(CODA_FIND_LIBLWP,
 [AC_CACHE_CHECK(location of liblwp, coda_cv_lwppath,
  [saved_LDFLAGS="${LDFLAGS}" ; saved_LIBS="${LIBS}"
   coda_cv_lwppath=none ; LIBS="-llwp"
   for path in /usr /usr/local /usr/pkg ; do
     LDFLAGS="${LDFLAGS} -L${path}/lib"
     AC_TRY_LINK([], [int main(){return 0;}],
                 [coda_cv_lwppath=${path} ; break])
   done
   LDFLAGS="${saved_LDFLAGS}" ; LIBS="${saved_LIBS}"])
 case $coda_cv_lwppath in
   none) AC_MSG_ERROR("Cannot determine the location of liblwp") ;;
   /usr) ;;
   *)    CPPFLAGS="${CPPFLAGS} -I${coda_cv_lwppath}/include"
         LDFLAGS="${LDFLAGS} -L${coda_cv_lwppath}/lib" ;;
 esac])
                                                
