dnl      --------  Adding a new system ----------
dnl Figure out what the GNU canonical name of your target is by
dnl running configure in the top directory
dnl   - add a configs/Makeconf.$sys file for your system
dnl   - add your VFS code to kernel-src/vfs/$vfsdir
dnl   - add a case statement below to set $sys and $vfsdir

case ${target} in

	*-*-djgpp )
		sys=win95
		initsuffix=../etc
 ;;
	*-*-cygwin32 )
		sys=cygwin32
		initsuffix=../etc
 ;;
	*-*-netbsd* )
	    	shortsys=nbsd
		sys=nbsd
		case ${host_cpu} in
			i*6 )   arch=i386 ;;
			sparc ) arch=sparc ;;
		esac
		vfsdir=bsd44
		os=`uname -r`
		initsuffix=../etc
 ;;

	*-*-freebsd2* )
		sys=i386_fbsd2
		vfsdir=bsd44
		initsuffix=../etc
 ;;

	*-*-freebsd3* )
		case `objformat` in
			aout) sys=i386_fbsd3 ;;
			elf)  sys=i386_fbsd2 ;;
		esac
		vfsdir=bsd44
		initsuffix=../etc
;;
	*-*-freebsd4* )
		sys=i386_fbsd2
		vfsdir=bsd44
		initsuffix=../etc
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
		if test -f /etc/debian_version
		then
			initsuffix=../etc/init.d
		else
			initsuffix=../etc/rc.d/init.d
		fi
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
		initsuffix=../etc/init.d
;;
esac
AC_SUBST(shortsys)
AC_SUBST(sys)
AC_SUBST(fullos)
AC_SUBST(os)
AC_SUBST(vfsdir)
AC_SUBST(initsuffix)
