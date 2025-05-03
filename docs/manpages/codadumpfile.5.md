---
title: CODADUMPFILE(5)
footer: Coda Distributed File System
---

## NAME

codadumpfile - Format of a dump file

## DESCRIPTION

The dump files are formatted as tagged byte streams. Each piece of the
dump is marked by a tag byte, a number between 1 and 20. Each piece may
also be broken into subpieces, each marked by a letter of the alphabet.
Each subpiece is uniquely marked, but tags can be reused in different
parts of the dump.

The first 60 bytes of the dump contain header information. This header
info identifies which backup volume was used to create the dump, and
when and from which volume the backup clone was created. It also
indicates whether the dump was incremental, and provides information to
be used later in placing this dump in the proper sequence for merging.
Following the dump header is the volume information structure used by
the Coda internals. Following this are two sequences of vnodes, one for
directories and one for files. Each sequence consists of a header and a
stream of vnodes.

The vnode sequence header contains the number of vnodes and the size of
the vnode list.  These numbers are not necessarily the same since not
every list needs to have a vnode on it and some lists may have more than
one. This is an artifact of the way vnodes are stored in the Coda
servers.

After these two fields comes a list of vnodes. Each vnode consists of
two parts, the first is the meta information associated with the file or
directory and the second is the data for that vnode. This data could
either be directory pages or file data. The first word after the tag for
the file or directory data is a count of bytes or directory pages. For
directory vnodes, the access list for that directory is included as part
of the meta information.

## SEE ALSO

**backup**(8), **dumplist**(5), **volutil**(8)

## AUTHOR

- Joshua Raiff, 1993
