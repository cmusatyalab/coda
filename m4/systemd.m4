#serial 1
dnl ---------------------------------------------------
dnl Determine systemd unit installation path.
dnl https://www.freedesktop.org/software/systemd/man/daemon.html#Installing%20Systemd%20Service%20Files
dnl
AC_DEFUN([SYSTEMD_CHECKS],
  [PKG_PROG_PKG_CONFIG
   AC_ARG_WITH([systemdsystemunitdir],
               [AS_HELP_STRING([--with-systemdsystemunitdir=DIR],
                               [Directory for systemd service files])],,
               [with_systemdsystemunitdir=auto])
   AS_IF([test "x$with_systemdsystemunitdir" = "xyes" -o "x$with_systemdsystemunitdir" = "xauto"],
         [def_systemdsystemunitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)
          AS_IF([test "x$def_systemdsystemunitdir" = "x"],
                [AS_IF([test "x$with_systemdsystemunitdir" = "xyes"],
                       [AC_MSG_ERROR([systemd support requested but pkg-config unable to query systemd package])])
                 with_systemdsystemunitdir=no],
                [with_systemdsystemunitdir="$def_systemdsystemunitdir"])
        ])
  AS_IF([test "x$with_systemdsystemunitdir" != "xno"],
        [AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])])
  AM_CONDITIONAL([HAVE_SYSTEMD], [test "x$with_systemdsystemunitdir" != "xno"])
])
