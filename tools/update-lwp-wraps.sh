#!/bin/sh
#
# Rewrite the lwp.wrap files used by the rpc2 and rvm subprojects to
# point at a specific lwp release. Run this after cutting a new lwp
# release, so the dormant (standalone-build) wraps track it.
#
# Copy the new release to subprojects/packagecache/lwp-<version>.tar.xz
#
# Usage: update-lwp-wraps.sh <version>
#
#   <version>      lwp release version, e.g. 2.19
#
set -e

if [ $# -ne 1 ] ; then
  echo "usage: $0 <version>" >&2
  exit 1
fi

VERSION="$1"

case "$VERSION" in
  '' | *[!0-9.]* )
    echo "$0: version is not a dotted number: $VERSION" >&2
    exit 1
    ;;
esac

TOPDIR=$(cd "$(dirname "$0")/.." && pwd)
TAR="$TOPDIR/subprojects/packagecache/lwp-$VERSION.tar.xz"

meson setup lwp-build "$TOPDIR/subprojects/lwp"
meson dist -C lwp-build
cp "lwp-build/meson-dist/lwp-$VERSION.tar.xz" "$TAR"
rm -r lwp-build

HASH=$(sha256sum "$TAR" | cut -d\  -f1)

if [ "${#HASH}" -ne 64 ] || ! printf '%s' "$HASH" | grep -qE '^[0-9a-fA-F]+$' ; then
  echo "$0: source_hash must be 64 hex chars (sha256)" >&2
  exit 1
fi

for wrap in \
  "$TOPDIR/subprojects/rpc2/subprojects/lwp.wrap" \
  "$TOPDIR/subprojects/rvm/subprojects/lwp.wrap" ; do
  cat > "$wrap" <<EOF
[wrap-file]
directory = lwp-$VERSION

source_url = http://coda.cs.cmu.edu/coda/source/lwp-$VERSION.tar.xz
source_filename = lwp-$VERSION.tar.xz
source_hash = $HASH

[provide]
dependency_names = lwp
EOF
  echo "updated $wrap -> lwp-$VERSION"
done
