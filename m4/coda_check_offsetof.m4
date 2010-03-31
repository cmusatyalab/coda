#serial 1
dnl ---------------------------------------------
dnl Test for possible offsetof implementation that doesn't give compile
dnl warnings or errors
dnl
AC_DEFUN([CODA_TEST_OFFSETOF],
   [AC_MSG_CHECKING(offsetof using $2)
    offsetof=no
    AC_LANG_PUSH(C++)
    saved_CPPFLAGS="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS -Werror"
    AC_RUN_IFELSE([
        AC_LANG_PROGRAM([[
            #include <sys/types.h>
            #include <stddef.h>
            struct b { struct b *C; };
            class A {public: A& operator=(const A& x){return *this;}; struct b B;};
            #define off(type,member) $2]],
            [[A *a=NULL; if (((char *)&a->B - off(A,B)) != (char *)a) return 1]])],
        [AC_DEFINE(CODA_OFFSETOF_$1, 1, [offsetof works with $2])
         offsetof=yes], [], [true])
    CPPFLAGS="$saved_CPPFLAGS"
    AC_LANG_POP(C++)
    AC_MSG_RESULT($offsetof)])

AC_DEFUN([CODA_CHECK_OFFSETOF],
   [CODA_TEST_OFFSETOF(OFFSETOF, [offsetof(type,member)])
    if test "$offsetof" != yes ; then
    CODA_TEST_OFFSETOF(PTR_TO_MEMBER, [((size_t)(&type::member))])
    if test "$offsetof" != yes ; then
    CODA_TEST_OFFSETOF(REINTERPRET_CAST, [((size_t)(&(reinterpret_cast<type*>(__alignof__(type*)))->member)-__alignof__(type*))])
    fi ; fi])

