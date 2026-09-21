# Building Coda

Coda is built with **Meson** and the Ninja backend.

## Prerequisites

- A C and C++ compiler (GCC or Clang).
- `bison` and `flex`, used to generate the rpc2 and resolver parsers
  and lexers.
- `pkg-config`.
- `lua` (optional; enables the rpc2 Lua rate-limiting scripts).
- **Meson >= 1.1.0** and **Ninja**.

The documentation `docs` target additionally needs `mkdocs` (with the
`mkdocs-material`, `mkdocs-awesome-nav`, `mkdocs-bibtex`, and
`mkdocs-minify-plugin` plugins); if `mkdocs` is not on the `PATH` it
falls back to `uvx`.

## Building

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

## Subprojects

`subprojects/lwp`, `subprojects/rpc2`, and `subprojects/rvm` are each
independent Meson projects with their own `meson.options`. They build as
part of Coda, and can also be built on their own by pointing `meson
setup` at the subproject directory:

```sh
meson setup /tmp/rpc2-build subprojects/rpc2
meson compile -C /tmp/rpc2-build
```

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
