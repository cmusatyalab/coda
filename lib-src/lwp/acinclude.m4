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

dnl ---------------------------------------------
dnl Check if makecontext works as expected

AC_DEFUN([CODA_CHECK_MAKECONTEXT],
   [AC_MSG_CHECKING(if makecontext correctly passes stack pointers)
    makecontext=no
    AC_TRY_RUN([
#if STDC_HEADERS
#include <stdlib.h>
#endif
#ifdef HAVE_UCONTEXT_H
#include <ucontext.h>
#endif
void *ptr;
void test(void *arg) { exit(ptr != arg); }
int main(int argc, char **argv) {
#ifdef HAVE_UCONTEXT_H
ucontext_t x;
static void *stack = NULL;
int arg = 0;
ptr = &arg;
getcontext(&x);
if (stack == NULL) {
#define SSIZE 32768
x.uc_stack.ss_sp = stack = malloc(SSIZE);
x.uc_stack.ss_size = SSIZE;
makecontext(&x, (void (*)(void))test, 1, &arg);
setcontext(&x);
}
#endif
exit(2);
}], [AC_DEFINE(CODA_USE_UCONTEXT, 1,
	      [makecontext correctly passes stack pointers])
     makecontext=yes])
  AC_MSG_RESULT($makecontext)])
