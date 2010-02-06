dnl Attempt to guess a nice sysconfdir
AC_MSG_CHECKING(configuration file location)
case ${prefix} in
    /usr )
	# AFAIK nobody has use for /usr/etc, it should simply be /etc.
	sysconfdir='${prefix}/../etc/coda'
	sysconfdirx="/etc/coda"
	;;

    */coda* )
	# If '/coda*' is already in the prefix, we don't need to add
	# a coda subdirectory to the sysconfdir.
	sysconfdir='${prefix}/etc'
	sysconfdirx="${prefix}/etc"
	;;

    * )
	# Otherwise append '/coda' so that we don't throw our configuration
	# files all over the place.
	sysconfdir='${prefix}/etc/coda'
	sysconfdirx="${prefix}/etc/coda"
	;;
esac
AC_MSG_RESULT(${sysconfdir})
AC_SUBST(sysconfdir)
AC_SUBST(sysconfdirx)

dnl      --------  Adding a new system ----------
dnl Figure out what the GNU canonical name of your target is by
dnl running configure in the top directory
dnl   - add a configs/Makeconf.$sys file for your system
dnl   - add a case statement below to set $sys

AC_MSG_CHECKING(cputype substitution)
cputype=${host_cpu}
case ${cputype} in
    i*6 )   cputype=i386 ;;
esac
AC_MSG_RESULT($cputype)
AC_SUBST(cputype)

AC_MSG_CHECKING(systype substitution)
shortsys=${host_os}
case ${shortsys} in
	cygwin32 )  shortsys=cygwin32 ;;
	netbsd* )   shortsys=nbsd ;;
	freebsd3* ) case `objformat` in
			aout) shortsys=fbsd_aout ;;
			elf)  shortsys=fbsd_elf ;;
		    esac
	;;
	freebsd* )  shortsys=fbsd_elf ;;
	linux* )    shortsys=linux ;;
	solaris2* ) shortsys=solaris2 ;;
esac
systype=${cputype}_${shortsys}
AC_MSG_RESULT(${systype})
AC_SUBST(systype)

