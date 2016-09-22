#######################################
# Rebuild RPC2 client and server stubs
#
# input: RPC2_FILES = *.rpc2
#
# output: BUILT_SOURCES, CLEANFILES, EXTRA_DIST, rp2gen rules/dependencies
#

_RP2GEN_GENERATED = ${RPC2_FILES:.rpc2=.h} ${RPC2_FILES:.rpc2=.client.c} \
		    ${RPC2_FILES:.rpc2=.server.c} ${RPC2_FILES:.rpc2=.multi.c} \
			${RPC2_FILES:.rpc2=.print.c} ${RPC2_FILES:.rpc2=.helper.c}

BUILT_SOURCES = ${RPC2_FILES:.rpc2=.h}
CLEANFILES = ${_RP2GEN_GENERATED} rp2gen.tmp rp2gen.stamp
EXTRA_DIST = ${RPC2_FILES}

rp2gen.stamp: ${RPC2_FILES} ${RP2GEN}
	@rm -f rp2gen.tmp
	@touch rp2gen.tmp
	@for file in ${RPC2_FILES} ; do \
	    echo "Generating RPC2 stubs for $$file" ; \
	    $(RP2GEN) -I $(srcdir) $(srcdir)/$$file ; done
	@mv -f rp2gen.tmp rp2gen.stamp

${_RP2GEN_GENERATED}: rp2gen.stamp
	@if test -f $@ ; then : ; else \
	    trap 'rm -rf rp2gen.lock rp2gen.stamp' 1 2 13 15 ; \
	    if mkdir rp2gen.lock 2>/dev/null ; then \
		rm -f rp2gen.stamp ; \
		$(MAKE) $(AM_MAKEFLAGS) rp2gen.stamp ; \
		rmdir rp2gen.lock ; \
	    else \
		while test -d rp2gen.lock ; do sleep 1 ; done ; \
		test -f rp2gen.stamp ; exit $$? ; \
	    fi ; \
	fi

