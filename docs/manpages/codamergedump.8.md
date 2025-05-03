---
title: CODAMERGEDUMP(8)
footer: Coda Distributed File System
---

## NAME

codamergedump - merge incremental dumps onto full dumps for restore

## SYNOPSIS

**codamergedump** output\_file full\_dump incremental\_dump

## DESCRIPTION

**codamergedump** is a utility that allows one to apply incremental
dumps to a full dump and produce a new full dump which reflects a later
state of the volume. The new dump can then be used to restore the
volume, or can be merged with other incrementals.

An incremental or full dump reflects the state of the volume at the time
it was dumped. Full dumps can be restored so that a user may access an
older state of a volume.  Incremental dumps do not necessarily have
sufficient information to be restored. For instance, all the vnodes in
the dump may not be present.  The merging process allows a full image of
a state that was only incrementally dumped to be restored.

Currently incrementals apply to the last successful full dump that was
done. As an example, say full dumps for a volume are done on Sundays,
with incrementals being taken the rest of the week. If an administrator
wishes to restore a volume to Wednesdays state, he would have to fetch
the full dumpfile from Sunday and the incremental dumpfile for
Wednesday. Once these are present, the administrator would use the
codamergedump program to create a new updated full dump for Wednesdays
state, and restore it to the server (using **volutil**(8) restore).

Information in the dump header is used to place a total ordering on the
dumps. This way incrementals can only be applied to the dump with repect
to which they were taken. In addition, incrementals cannot be applied to
other incrementals, and the dumps to be merged must have been created
from the same volume replica. The codamergedump program checks these
things, and reports failures if they are violated.

## SEE ALSO

**volutil**(8), **restore**(1)

## AUTHOR

- David C.\ Steere, 1991, created
