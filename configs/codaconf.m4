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
dnl   - add your VFS code to kernel-src/vfs/$vfsdir
dnl   - add a case statement below to set $sys and $vfsdir

case ${target} in

	*-*-djgpp )
		sys=win95
 ;;
	*-*-cygwin32 )
		sys=cygwin32
 ;;
	*-*-netbsd* )
	    	shortsys=nbsd
		sys=nbsd
		case ${host_cpu} in
			i*6 )   arch=i386 ;;
			sparc ) arch=sparc ;;
			arm32 ) arch=arm32 ;;
		esac
		vfsdir=bsd44
		os=`uname -r`
 ;;

	*-*-freebsd2* )
		sys=i386_fbsd2
		vfsdir=bsd44
 ;;

	*-*-freebsd3* )
		case `objformat` in
			aout) sys=i386_fbsd3 ;;
			elf)  sys=i386_fbsd2 ;;
		esac
		vfsdir=bsd44
 ;;
	*-*-freebsd* )
		sys=i386_fbsd2
		vfsdir=bsd44
 ;;

	*-*-linux-* )
		shortsys=linux
		sys=linux
		case ${host_cpu} in
			i*6 ) 	arch=i386 ;;
			sparc ) arch=sparc ;;
			alpha ) arch=alpha ;;
		 esac
		fullos=`uname -r`
		case ${fullos} in
			2.0.* ) os=2.0 ; vfsdir=linux ;;
			2.1.* ) os=2.1 ; vfsdir=linux21 ;;
			2.2.* )	os=2.2 ; vfsdir=linux21 ;;
		esac
 ;;
	*-*-solaris2* )
		shortsys=solaris2
		sys=solaris2
		case ${host_cpu} in
			i*6 )   arch=i386 ;;
			sparc ) arch=sparc ;;
		esac
		fullos=`uname -r`
		vfsdir=solaris2
 ;;
esac
AC_SUBST(shortsys)
AC_SUBST(sys)
AC_SUBST(fullos)
AC_SUBST(os)
AC_SUBST(vfsdir)
AC_SUBST(initsuffix)
