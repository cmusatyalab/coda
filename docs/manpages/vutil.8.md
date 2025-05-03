---
title: VUTIL(8)
footer: Coda Distributed File System
---

## NAME

vutil - venus utility program

## SYNOPSIS

**vutil** \[--clearstats | -cs] \[--debug loglevel | -d loglevel] \[--shutdown]
\[--stats] \[--swaplogs | -swap]

## DESCRIPTION

**vutil** dynamically alters various parameters of a running Coda client
by sending signals to the **venus** process.

## OPTIONS

**\--clearstats**, **-cs**

:   Clears internal counters used in statistics reporting.

**\--debug** *loglevel*, **-d** *loglevel*

:   Sets the debug level to *loglevel*. Additional arguments can be
    given to set log levels for **RPC2** and **LWP**.

**\--shutdown**

:   Shuts Venus down gracefully. Same as sending a **SIGTERM** signal.

**\--stats**, **-s**

:   Causes Venus to dump internal counters and other statistics to its
    log file.

**\--swaplogs**, **-swap**

:   Causes Venus to rotate the venus log and error files. Not quite the
    same as sending a **SIGHUP** signal because vutil performs the log
    rotation, while venus only reopens the files when it receives
    **SIGHUP**.

## DIAGNOSTICS

**vutil** is using signals to control the Coda client. As a result it
has to run as the super user or the same user as the **venus** process.

## FILES

**vutil** will read the location of the pid file, control file, and log
files from `/etc/coda/venus.conf`.

## SEE ALSO

**venus**(8)

## AUTHOR

- Jay Kistler, 1990, Created
- Jan Harkes, 2006, Updated
