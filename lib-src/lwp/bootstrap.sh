#!/bin/sh
#
# Running this should create the following files:
# 
# touch      - ChangeLog
# autoheader - config.h.in
# aclocal    - aclocal.m4
# automake   - COPYING INSTALL Makefile.in config.guess config.sub install-sh
#              libtool ltconfig ltmain.sh missing mkinstalldirs stamp-h.in
# autoconf   - configure
#

touch ChangeLog
autoheader
aclocal
automake --copy --add-missing
autoconf
