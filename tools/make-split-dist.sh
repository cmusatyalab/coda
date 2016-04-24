#!/bin/sh
#
# Build separate lwp-, rpc2-, rvm- and coda-tarballs.
#
set -e

make -C lib-src/lwp dist
make -C lib-src/rpc2 dist
make -C lib-src/rvm dist
make subdirs="" dist

mv lib-src/lwp/lwp-*.tar.gz .
mv lib-src/rpc2/rpc2-*.tar.gz .
mv lib-src/rvm/rvm-*.tar.gz .

