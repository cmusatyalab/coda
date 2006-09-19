dnl Attempt to guess a nice sysconfdir
AC_MSG_CHECKING(configuration file location)
case ${prefix} in
    /usr )
	# AFAIK nobody has use for /usr/etc, it should simply be /etc.
	sysconfdir='/etc/coda'
	initsuffix='../etc'
	;;

    */coda* )
	# If '/coda*' is already in the prefix, we don't need to add
        # a coda subdirectory to the sysconfdir.
	sysconfdir=${prefix}/etc
	initsuffix='etc'
	;;

    * )
	# Otherwise append '/coda' so that we don't throw our configuration
	# files all over the place.
	if test "x${prefix}" != xNONE ; then
		sysconfdir=${prefix}/etc/coda
	else
		sysconfdir=${ac_default_prefix}/etc/coda
	fi
	initsuffix='etc'
	;;
esac
AC_MSG_RESULT(${sysconfdir})
AC_SUBST(sysconfdir)

dnl Now the initdir isn't finished yet, we have to figure out where the
dnl system we're building on wants the init scripts.

AC_MSG_CHECKING(init script location)
if test $cross_compiling = yes ; then
    # probably WinXX, existing ${initsuffix} should be fine.
    AC_MSG_RESULT(cross compiling, using ${initsuffix})

elif test -d ${prefix}/${initsuffix}/init.d -a ! -h ${prefix}/${initsuffix}/init.d ; then
    # probably Debian or Solaris, or other SysV standard setup.
    initsuffix=${initsuffix}/init.d
    AC_MSG_RESULT(standard SysV)

elif test -d ${prefix}/${initsuffix}/rc.d/init.d ; then
    # probably RedHat-x.x's SysV derived setup.
    AC_MSG_RESULT(RedHat)
    initsuffix=${initsuffix}/rc.d/init.d

elif test -d ${prefix}/${initsuffix}/rc.d ; then
    # probably FreeBSD or NetBSD's BSD-style init-scripts.
    AC_MSG_RESULT(BSD style)
    initsuffix=${initsuffix}/rc.d
fi
AC_SUBST(initsuffix)

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

dnl -- should we add -R flags to LDFLAGS?

echo -n "Checking whether ld needs the -R flag... "
case ${shortsys} in
	nbsd ) if test x`echo __ELF__ | gcc -E - | grep ELF` != x__ELF__ ; then
		   RFLAG=1
	       fi;;
	solaris2 ) RFLAG=1 ;;
esac

if test x${RFLAG} != x ; then
	echo yes
else
	echo no
fi

