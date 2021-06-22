#!/bin/sh
#
# Check if configure.ac versions match up with git tags and
# make sure that all subsystems have an uptodate release tag
# when we're building a new Coda release
#
# Also check that the meson.build files carry the same project
# and libtool versions as the autotools files
#

TAGGED_SUBSYS=$(echo "$CI_COMMIT_TAG" | cut -d- -f1)

ACINIT_RE='^AC_INIT(\[[^]]*\],\[\([^]]*\)\].*'

checkver () {
  SUBSYS="$1"
  SUBDIR="$2"

  # VERSION is git version (latest tag + # commits + commit sha)
  # RELEASE is latest git tagged version
  # CONFVER is configure.ac version
  VERSION=$(git describe --match="$SUBSYS-*" | cut -d- -f2-)
  RELEASE=$(echo "$VERSION" | cut -d- -f1)
  CONFVER=$(sed -n "s/$ACINIT_RE/\1/p" "$SUBDIR/configure.ac")

  # the configure.ac version should match the tagged version
  if [ "$CONFVER" != "$RELEASE" ] ; then
    echo "$SUBSYS: configure.ac version does not match git tag"
    [ "$TAGGED_SUBSYS" = "$SUBSYS" ] && exit 1
  fi

  # are there changes since the last release tag?
  if [ -n "$(git diff "$SUBSYS-$RELEASE" "$SUBDIR")" ]
  then
    echo "$SUBSYS: untagged version $VERSION"
    [ "$TAGGED_SUBSYS" = "coda" ] && exit 1
  fi

  # the meson.build project version should match configure.ac
  MESONVER=$(sed -n "s/^  version : '\([^']*\)',\{0,1\}$/\1/p" "$SUBDIR/meson.build")
  if [ -n "$MESONVER" ] && [ "$MESONVER" != "$CONFVER" ] ; then
    echo "$SUBSYS: meson.build version $MESONVER does not match configure.ac version $CONFVER"
    [ "$TAGGED_SUBSYS" = "$SUBSYS" ] && exit 1
    [ "$TAGGED_SUBSYS" = "coda" ] && exit 1
  fi

  # the meson libtool version should match CODA_LIBRARY_VERSION in configure.ac
  AUTOLT=$(sed -n "s/^CODA_LIBRARY_VERSION(\([0-9]*\), \([0-9]*\), \([0-9]*\)).*/\1:\2:\3/p" "$SUBDIR/configure.ac")
  if [ -n "$AUTOLT" ] ; then
    MESOLT=$(sed -n "s/^lt_revision = \([0-9]*\)$/\1/p" "$SUBDIR/meson.build")
    MESOLT="$MESOLT:$(sed -n "s/^lt_current = \([0-9]*\)$/\1/p" "$SUBDIR/meson.build")"
    MESOLT="$MESOLT:$(sed -n "s/^lt_age = \([0-9]*\)$/\1/p" "$SUBDIR/meson.build")"
    if [ "$MESOLT" != "$AUTOLT" ] ; then
      echo "$SUBSYS: libtool version mismatch: configure.ac $AUTOLT vs meson.build $MESOLT"
      [ "$TAGGED_SUBSYS" = "$SUBSYS" ] && exit 1
      [ "$TAGGED_SUBSYS" = "coda" ] && exit 1
    fi
  fi
}

checkver coda .
checkver lwp subprojects/lwp
checkver rpc2 subprojects/rpc2
checkver rvm subprojects/rvm
exit 0
