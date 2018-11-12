#!/bin/sh

ACINIT_RE='^\(AC_INIT([^,]*, \)\([^,]*\)\(.*)\)'
fixver () {
  SUBSYS="$1"

  VERSION=$(git describe --match="$SUBSYS-*" | cut -d- -f2-)
  RELEASE=$(echo $VERSION | cut -d- -f1)

  # any changes to the subdir since the last release tag?
  [ -n "$(git diff $SUBSYS-$RELEASE lib-src/$SUBSYS)" ] || VERSION="$RELEASE"

  echo "$SUBSYS-$VERSION"
  sed -i "s/$ACINIT_RE/\1$VERSION\3/" lib-src/$SUBSYS/configure.ac
}

# Fix up Coda, LWP, RPC2, and RVM versions
if [ "$1" = "--fix-versions" ] ; then
  CODA_VERSION=$(git describe --match="coda-*" | cut -d- -f2-)

  echo "coda-$CODA_VERSION"
  sed -i "s/$ACINIT_RE/\1$CODA_VERSION\3/" configure.ac

  fixver lwp
  fixver rpc2
  fixver rvm
fi

# Update submodules
git submodule init external-src/googletest
git submodule update external-src/googletest

# and rebuild configure files
autoreconf --verbose --install --force
