#!/bin/sh
#
# Running this should create the following files:
# 
# autoconf   - configure
#

rm -rf autom4te.cache
touch README
autoheader
aclocal
libtoolize --copy --force
automake --add-missing --copy
autoconf
