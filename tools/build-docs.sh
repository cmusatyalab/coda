#!/bin/sh

[ -z "$MESON_SOURCE_ROOT" ] && exit 1
[ -z "$MESON_BUILD_ROOT" ] && exit 1

cd "$MESON_BUILD_ROOT" || exit 1

# copying all the files also works around some issues with symlinks
rsync -a --copy-links "$MESON_SOURCE_ROOT/docs/" docs/
rsync -a --copy-links "$MESON_SOURCE_ROOT/docs-meta/" docs-meta/
rsync -a --copy-links "$MESON_SOURCE_ROOT/mkdocs.yml" mkdocs.yml

uvx \
  --with mkdocs-material \
  --with mkdocs-awesome-nav \
  --with mkdocs-bibtex \
  --with mkdocs-minify-plugin \
  mkdocs build
