#!/bin/sh
#
# Check that the meson.build files carry the same project version as
# their git tags, and make sure that all subsystems have an up-to-date
# release tag when we're building a new Coda release
#

TAGGED_SUBSYS=$(echo "$CI_COMMIT_TAG" | cut -d- -f1)

checkver () {
  SUBSYS="$1"
  SUBDIR="$2"

  # VERSION is git version (latest tag + # commits + commit sha)
  # RELEASE is latest git tagged version
  VERSION=$(git describe --match="$SUBSYS-*" | cut -d- -f2-)
  RELEASE=$(echo "$VERSION" | cut -d- -f1)

  # are there changes since the last release tag?
  if [ -n "$(git diff "$SUBSYS-$RELEASE" "$SUBDIR")" ]
  then
    echo "$SUBSYS: untagged version $VERSION"
    [ "$TAGGED_SUBSYS" = "coda" ] && exit 1
  fi

  # the meson.build project version should match the git tag
  MESONVER=$(sed -n "s/^[[:space:]]*version:[[:space:]]*'\([^']*\)',\{0,1\}$/\1/p" "$SUBDIR/meson.build")
  if [ -n "$MESONVER" ] && [ "$MESONVER" != "$RELEASE" ] ; then
    echo "$SUBSYS: meson.build version $MESONVER does not match git tag $RELEASE"
    [ "$TAGGED_SUBSYS" = "$SUBSYS" ] && exit 1
    [ "$TAGGED_SUBSYS" = "coda" ] && exit 1
  fi
}

checkver coda .
checkver lwp subprojects/lwp
checkver rpc2 subprojects/rpc2
checkver rvm subprojects/rvm
exit 0
