---
title: CODA-VOLMUNGE(1)
footer: Coda Distributed File System
---

## NAME

coda-volmunge - Walk a Coda tree to resolve conflicts, identify mountpoints, etc.

## SYNOPSIS

**coda-volmunge** [-adflmuioqvx] directory...

## DESCRIPTION

**coda-volmunge** is ideal for identifying Coda objects versus regular UNIX
files (including UNIX directories) stored within the Coda filesystem. It
will work recursively.

The **-f** and **-o** options explicitly call the stat() and open()
functions respectively, resolution can be forced with volmunge if one is
rebuilding a replicated Coda volume or group of volumes mounted on top
of each other.

However, it is expected that stat() will be called on all objects either
way because Coda does not provide directory vs. file information in the
readdir() result so any volume traversal will trigger automatic resolution.

## OPTIONS

-a, \-\-all

:   Prints out everything.

-d, \-\-directories

:   Prints out UNIX directories, but not volume mount points.

-f, \-\-files

:   Prints out all objects which are not volume mount points (eg UNIX
    files and symlinks); this performs a stat() call on all non-volume
    objects which is ideal for forcing resolution on a volume.

-m, \-\-mountpoints

:   Prints out those objects which are volume mount points.

-u, \-\-mountlinks

:   Prints out those objects which are unmounted volume mount points
    (mount-links).

-i, \-\-inconsistent

:   Prints out those objects which indicate there is an unrepaired
    conflict (inconsistent object).

-o, \-\-open-files

:   Perform and open() call on all UNIX files, forces a fetch into local
    Coda cache.

-q, \-\-quiet

:   Be less verbose.

-v, \-\-verbose

:   Be more verbose.

-x, \-\-cross-volume

:   Cross volume boundaries.

-z, \-\-is-empty

:   Prints out those objects which are empty files or directories.

## SEE ALSO

**find**(1)

## AUTHOR

Henry M. Pierce, 1998, created.

Jan Harkes, 2021, rewrote volmunge to Python.
