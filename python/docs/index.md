# CodaFS Python module

Python helpers for accessing Coda File System functionality.

This is a collection of scripts and modules for working with Coda.

## Installation

The CodaFS Python module can be installed from PyPI.org. It is recommended to
install it locally in an isolated environment with **pipx** or **uv**.

=== "pip"
        pip install --user codafs

=== "pipx"
    See also <https://pipx.pypa.io/latest/installation/>

        pipx ensurepath
        pipx install codafs

=== "uv"
    See also <https://docs.astral.sh/uv/getting-started/installation/>

        uv tool install codafs

## Development

See [https://github.com/cmusatyalab/coda.git/python/](https://github.com/cmusatyalab/coda/tree/master/python/).

## Commands

* [coda-make-certs](manpages/make_certs.1.md) - Generate X509 certificates for
  Coda realms and servers
* [coda-sync-acls](manpages/sync_acls.1.md) - Copy Coda ACLs from a source
  directory tree or a serialized CodaACLs.yaml file.
* [coda-volmunge](manpages/volmunge.1.md) - Walk a Coda tree to resolve
  conflicts, identify mountpoints, etc.
