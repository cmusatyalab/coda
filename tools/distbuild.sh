#!/bin/sh

cd "$MESON_DIST_ROOT"

# Regenerate automake/autoconf files
./bootstrap.sh

# Clean up
rm -r autom4te.cache
rm -r external-src
