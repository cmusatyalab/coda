#!/bin/sh
#
# Running this should create the following files:
# 
# autoconf   - configure
#

rm -rf autom4te.cache
autoheader
autoconf
