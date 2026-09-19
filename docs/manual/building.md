# Building Coda

Coda builds with two supported build systems: the traditional
autotools (`bootstrap`/`configure`/`make`) build and a faster Meson
build. Both are kept in working order and exercised by CI. For
iterative development the Meson build is preferred: it configures and
links much faster and generates fewer intermediate files.

## Prerequisites

- A C and C++ compiler (GCC or Clang).
- `bison` and `flex`, used to generate the rpc2 and resolver parsers
  and lexers (required by both builds).
- `pkg-config`.
- `lua` (optional; enables the rpc2 Lua rate-limiting scripts).

The Meson build additionally requires **Meson >= 1.1.0** and
**Ninja**. The documentation `docs` target also needs `uv`/`uvx`.

## Autotools

```sh
./bootstrap.sh                              # regenerate build scripts
./configure --prefix=/usr --with-lua       # --disable-unit-test to skip tests
make
make install
make check                                 # GoogleTest unit tests
```

## Meson

```sh
meson setup build --fatal-meson-warnings   # configure
meson compile -C build                     # build
meson test -C build                        # GoogleTest unit tests
meson install -C build                     # install
```

Each project's `meson.build` records its own `meson_version` floor; taken
together, building all of Coda needs **Meson >= 1.1.0**. That is a
compatibility *floor*, not the version you develop with: use the newest
Meson available, and pass `--fatal-meson-warnings` so configure fails if
a feature newer than the floor is used. This keeps the floor honest
without pinning a specific release.

To run a single test suite:

```sh
build/test-src/unit/unit --gtest_filter=tcpftp.*
```

Relevant options (see `meson.options`): `-Dbuild_client`,
`-Dbuild_server`, `-Dvcodacon`, `-Dsystemd`, plus the
`-Dsystemdsystemunitdir` and `-Dmodulesloaddir` path overrides.

## Install tags

Meson installables are grouped into install *tags*. A bare
`meson install` installs everything; pass `--tags` to select a subset.

!!! note

    `--no-tags` is not available in Meson, so an exclusion such as
    "everything except the development files" is expressed as a positive
    `--tags` list.

| Tag       | Contents                                                                          |
| --------- | --------------------------------------------------------------------------------- |
| `runtime` | Versioned shared libraries, executables, `sbin` scripts, `/etc/coda` configuration, systemd units, the CA certificate, Lua scripts, and the modules-load.d config |
| `man`     | Man pages                                                                          |
| `devel`   | Public headers, pkg-config files, static libraries, the unversioned `.so` development symlink, and the `rp2gen` compiler |

Common installs:

```sh
# everything (default)
meson install -C build

# a normal install, without the development files
meson install -C build --tags runtime,man

# the development files only
meson install -C build --tags devel
```

## Documentation

The manual and man-page reference are generated with MkDocs:

```sh
ninja -C build docs                        # writes to <builddir>/site
```
