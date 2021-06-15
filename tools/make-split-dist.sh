#!/bin/sh
#
# Build separate lwp-, rpc2-, rvm- and coda-tarballs.
#
set -e

make -C subprojects/lwp dist
make -C subprojects/rpc2 dist
make -C subprojects/rvm dist
make subdirs="" dist

mv subprojects/lwp/lwp-*.tar.gz .
mv subprojects/rpc2/rpc2-*.tar.gz .
mv subprojects/rvm/rvm-*.tar.gz .
