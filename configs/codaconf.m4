dnl Attempt to guess a nice sysconfdir
case ${prefix} in
    /usr )
	# AFAIK nobody has use for /usr/etc, it should simply be /etc.
	sysconfdir='/etc/coda'
	initsuffix='../etc'
	;;

    */coda* )
	# If '/coda.*' is already in the prefix, we don't need to do anything
	# for the sysconfdir.
	initsuffix='etc'
	;;

    * )
	# Otherwise append '/coda' so that we don't throw our configuration
	# files all over the place.
	sysconfdir=${sysconfdir}/coda
	initsuffix='etc'
	;;
esac

dnl Now the initdir isn't finished yet, we have to figure out where the
dnl system we're building on wants the init scripts.

if test $cross_compiling = yes ; then
    # probably WinXX, existing ${initsuffix} should be fine.
    true
elif test -d ${prefix}/${initsuffix}/init.d ; then
    # probably Debian or Solaris, or other SysV standard setup.
    initsuffix=${initsuffix}/init.d

elif test -d ${prefix}/${initsuffix}/rc.d/init.d ; then
    # probably RedHat-x.x's SysV derived setup.
    initsuffix=${initsuffix}/rc.d/init.d

elif test -d ${prefix}/${initsuffix}/rc.d ; then
    # probably FreeBSD or NetBSD's BSD-style init-scripts.
    initsuffix=${initsuffix}/rc.d
fi

dnl      --------  Adding a new system ----------
dnl Figure out what the GNU canonical name of your target is by
dnl running configure in the top directory
dnl   - add a configs/Makeconf.$sys file for your system
dnl   - add a case statement below to set $sys

cputype=${host_cpu}
case ${cputype} in
    i*6 )   cputype=i386 ;;
esac

shortsys=${host_os}
case ${shortsys} in
	djgpp )     shortsys=win95 ;;
	cygwin32 )  shortsys=cygwin32 ;;
	netbsd* )   shortsys=nbsd ;;
	freebsd3* ) case `objformat` in
			aout) shortsys=fbsd_aout ;;
			elf)  shortsys=fbsd_elf ;;
		    esac
	;;
	freebsd* )  shortsys=fbsd_elf ;;
	linux-* )   shortsys=linux ;;
	solaris2* ) shortsys=solaris2 ;;
esac

systype=${cputype}_${shortsys}

AC_SUBST(initsuffix)
AC_SUBST(cputype)
AC_SUBST(shortsys)
AC_SUBST(systype)

