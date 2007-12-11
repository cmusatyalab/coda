#serial 2
dnl ---------------------------------------------
dnl Define library version
dnl
dnl For library version updates, follow these steps in order.
dnl - when any library source has changed, increment first
dnl - when interfaces were added/removed changed, increment second and set
dnl   first to 0
dnl - if any interfaces were added, increment third
dnl - if any interfaces were removed, set third to 0
dnl
dnl CODA_LIBRARY_VERSION(1, 2, 3)
dnl
AC_SUBST(LIBTOOL_VERSION)
AC_SUBST(MAJOR_VERSION)
AC_SUBST(LINUX_VERSION)
AC_SUBST(DLL_VERSION)
AC_SUBST(FREEBSD_VERSION)
AC_SUBST(GENERIC_VERSION)
AC_SUBST(LIBTOOL_LDFLAGS)
AC_DEFUN([CODA_LIBRARY_VERSION],
  [major=`expr $2 - $3`
   MAJOR_VERSION="$major"
   LINUX_VERSION="$major.$3.$1"
   DLL_VERSION="$major-$3-$1"
   FREEBSD_VERSION="$2"
   GENERIC_VERSION="$2.$1"
   LIBTOOL_LDFLAGS="-version-info $2:$1:$3 -no-undefined"])


