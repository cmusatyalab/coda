---
title: SERVERS(5)
footer: Coda Distributed File System
---

## NAME

servers - map server names to numbers

## DESCRIPTION

Each file server has a file, `/vice/db/servers`, which maps server names
to numbers. The server numbers are 8 bit numbers so a maximum of 256
servers can be supported. Once a server number has been assigned, it may
**not** be reused. You should not reassign a server a new id without
initializing the server.

The server numbers are used in too many places, probably the most
important is in the creation of volumes. The server creating a volume
uses its server number as the first byte of the four-byte volume
identification number in order to ensure uniqueness.

NOTE: Replicated volume numbers must also be unique, so we reserve part
of the range, 0 and 127 (00 and 7F hex), for this purpose. Real server numbers
must **not** be allocated from these subranges, and vice-versa.

The format of the file is the full server name (e.g.
mahler.coda.cs.cmu.edu) followed by a tab and then the servers number.
Any line beginning with a "#" is considered a comment and is ignored.

The following is an example servers file:

    MAHLER.CODA.CS.CMU.EDU          201
    GRIEG.CODA.CS.CMU.EDU           204
    HAYDN.CODA.CS.CMU.EDU           205
    WAGNER.CODA.CS.CMU.EDU          206
    DEBUSSY.CODA.CS.CMU.EDU         212
    SCARLATTI.CODA.CS.CMU.EDU       214
    ROSSINI.CODA.CS.CMU.EDU         215
    PUCCINI.CODA.CS.CMU.EDU         216
    GERSHWIN.CODA.CS.CMU.EDU        218
    SCHUMANN.CODA.CS.CMU.EDU        219

## FILES

- `/vice/db/servers`

## BUGS

The server numbers and replicated volume prefixes are pulled from the
same number space. Separation is enforced only by convention. This file
and `/vice/db/hosts` should be combined.

## SEE ALSO

**vrdb**(5), **vsgdb**(5), **maxgroupid**(5), **hosts**(5)

## AUTHOR

- Maria R.\ Ebling, 1990, Created
