#serial 1
dnl ---------------------------------------------
dnl Configure a subdirectory without recursively adding
dnl Makefile (e.g. make targets) into the build process.
dnl Useful to prevent building unnecessary targets from
dnl external repositories.
dnl
AC_DEFUN([CODA_SOFT_CONFIG_SUBDIR], [
    if test -d "$1"; then
        if test -f "$1/configure.ac"; then
            autoreconf --verbose --install --no-recursive "$1"
        fi

        if test -f "$1/configure"; then
            (cd "$1" && ./configure $2)
        fi
    fi
])

