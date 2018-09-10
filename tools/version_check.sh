#!/bin/sh
#
# Check if configure.ac versions match up with git tags and
# make sure that all subsystems have an uptodate release tag
# when we're building a new Coda release
#

TAGGED_SUBSYS=$(echo $CI_COMMIT_TAG | cut -d- -f1)

ACINIT_RE='^\(AC_INIT([^,]*, \)\([^,]*\)\(.*)\)'

checkver () {
  SUBSYS="$1"
  SUBDIR="$2"

  # VERSION is git version (latest tag + # commits + commit sha)
  # RELEASE is latest git tagged version
  # CONFVER is configure.ac version
  VERSION=$(git describe --match="$SUBSYS-*" | cut -d- -f2-)
  RELEASE=$(echo $VERSION | cut -d- -f1)
  CONFVER=$(sed -n "s/$ACINIT_RE/\2/p" $SUBDIR/configure.ac)

  # the configure.ac version should match the tagged version
  if [ "$CONFVER" != "$RELEASE" ] ; then
    echo "$SUBSYS: configure.ac version does not match git tag"
    [ "$TAGGED_SUBSYS" = "$SUBSYS" ] && exit 1
  fi

  # are there changes since the last release tag?
  if [ -n "$(git diff $SUBSYS-$RELEASE $SUBDIR)" ]
  then
    echo "$SUBSYS: untagged version $VERSION"
    [ "$TAGGED_SUBSYS" = "coda" ] && exit 1
  fi
}

checkver coda .
checkver lwp lib-src/lwp
checkver rpc2 lib-src/rpc2
checkver rvm lib-src/rvm
exit 0
