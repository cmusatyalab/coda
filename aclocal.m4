AC_DEFUN(CODA_CC_FNO_EXCEPTIONS,
[AC_CACHE_CHECK(whether the C compiler accepts -fno-exceptions,
coda_cv_cc_fno_exceptions,
[cat > conftest.c <<EOF
void main(void) { exit(0); }
EOF
if AC_TRY_COMMAND(${CC-cc} -fno-exceptions -o conftest conftest.c); then
  coda_cv_cc_fno_exceptions=yes
else
  coda_cv_cc_fno_exceptions=no
fi])
if test $coda_cv_cc_fno_exceptions = yes; then
  CC="$CC -fno-exceptions"
fi])

AC_DEFUN(CODA_CXX_FNO_EXCEPTIONS,
[AC_CACHE_CHECK(whether the C++ compiler accepts -fno-exceptions,
coda_cv_cxx_fno_exceptions,
[cat > conftest.c <<EOF
void main(void) { exit(0); }
EOF
if AC_TRY_COMMAND(${CXX-c++} -fno-exceptions -o conftest conftest.c) ; then
  coda_cv_cxx_fno_exceptions=yes
else
  coda_cv_cxx_fno_exceptions=no
fi])
if test $coda_cv_cxx_fno_exceptions = yes; then
  CXX="$CXX -fno-exceptions"
fi])
