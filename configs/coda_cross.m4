dnl First do a simple substitution to make life easier for `config.guess'.

case ${target} in
    cygwin32 | winnt )
	target=i386-pc-cygwin32
	;;
    arm ) 
	target=arm-unknown-linux-gnuelf 
	;;
esac

dnl Now let configure fill in the blanks.
AC_CANONICAL_SYSTEM

dnl Make build/host/target consistent again
host=${target}
host_alias=${target_alias}
host_cpu=${target_cpu}
host_os=${target_os}
host_vendor=${target_vendor}

dnl And set the build-tools when we are cross-compiling.
if test ${build} != ${host} ; then
    case ${host} in
	i386-pc-cygwin32 )
	    CC="gnuwin32gcc"
	    CXX="gnuwin32g++"
	    AR="gnuwin32ar"
	    RANLIB="gnuwin32ranlib"
	    AS="gnuwin32as"
	    LDFLAGS="-L/usr/gnuwin32/lib"
	    ;;
	arm-unknown-linux-gnuelf )
	    CROSS_COMPILE="arm-unknown-linuxelf-"
	    ;;
    esac
fi

if test "${CROSS_COMPILE}" ; then
    CC=${CROSS_COMPILE}gcc
    CXX=${CROSS_COMPILE}g++
    CFLAGS="-DHAVE_MMAP ${CFLAGS}"
    CXXFLAGS="-DHAVE_MMAP ${CFLAGS}"
    AR=${CROSS_COMPILE}ar
    RANLIB=${CROSS_COMPILE}ranlib
    AS=${CROSS_COMPILE}as
    NM=${CROSS_COMPILE}nm
    OBJDUMP=${CROSS_COMPILE}objdump
fi

