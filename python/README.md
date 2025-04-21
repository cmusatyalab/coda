# CodaFS Python helpers

Python helpers for the Coda Distributed File System.

Some wrappers around Coda command line interface tooling to make it easier to
access Coda specific information from the file system. Things like the unique
Coda FID (file identifier) and manipulating Coda ACLs (access control lists).

And a collection of Python scripts that use these wrappers to help with the
management of the file system.

* coda-make-certs is used to generate and update X509 certificates for Coda
  realms and servers.
* coda-sync-acls can be used to copy, backup and restore ACLs.
* coda-volmunge is a tool to walk a subtree of a volume to trigger the client
  to make sure all replicas are up-to-date and force server-server resolution
  in case of discrepancies. It can also be used to find unresolvable conflicts,
  missing volume mounts and other oddities.

This is a work in progress.

## Installation

```sh
pipx install codafs
```

## Development

```sh
git clone https://github.com/cmusatyalab/coda.git
cd coda/python
pipx install -e .
```

Rebuild the sdist and wheel packages with pyproject-build.

```sh
pipx install build
cd coda/python
pyproject-build
```

## Documentation / Manual pages

This documentation can be built using mkdocs-material.

```sh
pipx install mkdocs-material --install-deps
cd coda/python
mkdocs serve
```

The man pages under `coda/python/docs/manpages` can also be converted to UNIX
man pages using pandoc.

```sh
cd coda/python/docs/manpages
pandoc -s -t man filename.1.md > coda-filename.1
```
