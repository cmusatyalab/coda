#serial 2
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

   AC_ARG_WITH([modulesloaddir],
               [AS_HELP_STRING([--with-modulesloaddir=DIR],
                               [Directory for systemd's boot time kernel module configuration])],,
               [with_modulesloaddir=auto])
   AS_IF([test "x$with_modulesloaddir" = "xyes" -o "x$with_modulesloaddir" = "xauto"],
         [def_modulesloaddir=$($PKG_CONFIG --variable=modulesloaddir systemd)
          AS_IF([test "x$def_modulesloaddir" = "x"],
                [AS_IF([test "x$with_modulesloaddir" = "xyes"],
                       [AC_MSG_ERROR([systemd support requested but pkg-config unable to query systemd package])])
                 with_modulesloaddir=no],
                [with_modulesloaddir="$def_modulesloaddir"])
        ])
   AS_IF([test "x$with_modulesloaddir" != "xno"],
         [AC_SUBST([modulesloaddir], [$with_modulesloaddir])])

   AM_CONDITIONAL([HAVE_SYSTEMD], [test "x$with_systemdsystemunitdir" != "xno"])
])
