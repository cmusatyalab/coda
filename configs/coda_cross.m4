dnl First do a simple substitution to make life easier for `config.guess'.

case ${target} in
    djgpp | win95 )
	target=i386-pc-djgpp
	;;
    cygwin32 | winnt )
	target=i386-pc-cygwin32
	;;
esac

dnl Let configure fill in the blanks.
AC_CANONICAL_SYSTEM

dnl And override/set the build-tools when we are cross-compiling.
if test ${build} != ${target} ; then
    case ${target} in

	i386-pc-djgpp )
	    CC="dos-gcc"
	    CXX="dos-gcc"
	    AR="dos-ar"
	    RANLIB="true"
	    AS="dos-as"

	    CFLAGS="-bmmap -DDOS"
	    ;;

	i386-pc-cygwin32 )
	    CC="gnuwin32gcc"
	    CXX="gnuwin32g++"
	    AR="gnuwin32ar"
	    RANLIB="gnuwin32ranlib"
	    AS="gnuwin32as"
	    LDFLAGS="-L/usr/gnuwin32/lib"
	    ;;
    esac
fi

