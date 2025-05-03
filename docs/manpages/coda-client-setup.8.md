---
title: CODA-CLIENT-SETUP(8)
footer: Coda Distributed File System
---

## NAME

coda-client-setup - setup a Coda client (venus)

## SYNOPSIS

**coda-client-setup** \[default.realm.name] \[cachesize in kb]

## DESCRIPTION

The **coda-client-setup** command takes a default Coda realm name and a
cache size given in kilobytes. **coda-client-setup** then performs the
following operations:

- Creates any missing directories, for example the `/coda` mount point.
- Creates `/etc/coda/venus.conf` and sets the cache size given on the command
  line.
- Creates `/dev/cfs0` if not already present.

## EXAMPLE

``` sh
# coda-client-setup testserver.coda.cs.cmu.edu 100000
```

Sets up, `/etc/coda/venus.conf`, so that Coda will as a default
authenticate against the *testserver.coda.cs.cmu.edu* realm and
configures **venus** with a 100MB cache.

## BUGS

The \[cachesize in kb] option to coda-client-setup is not very smart.
In fact, it is quite dumb. No abbreviations are allowed after the number
and the number is taken literally to be kilobytes.

## SEE ALSO

**venus**(8)

Coda File System User and System Administator Manual: Installing A Coda Client

## AUTHOR

- Henry M. Pierce, 1998, created
