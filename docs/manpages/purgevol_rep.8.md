---
title: PURGEVOL_REP(8)
footer: Coda Distributed File System
---

## NAME

purgevol\_rep - delete a replicated volume

## SYNOPSIS

**purgevol\_rep** volumename

## DESCRIPTION

**purgevol\_rep** is a front-end to the `volutil purgevol_rep` command
and is used to delete a replicated volume from the Coda system.
**purgevol\_rep** determines the replicated volumeIds corresponding to
this volume by examining the readable version of the *VRDB* located in
`/vice/vol/VRList`. It then uses the *volutil* utility to purge the
individual replicas at the sites of replication. Next, it removes the
entry for the deleted volume from the `/vice/vol/VRList` and builds a
new *VRDB*. Finally, it builds a new *VLDB* using the **bldvldb.sh**(8)
script. Like the other volume utilities, **purgevol\_rep** must be run
on the SCM by root.

## FILES

- `/vice/vol/AllVolumes` - updated by removing the entries for the replicas of
  the purged volume.
- `/vice/vol/VolumeList` - updated by the remote server as a side effect of
  purging the volume.
- `/vice/vol/VRList` - updated by deleting the entry for the purged volume.

## SEE ALSO

**bldvldb.sh**(8), **purgevol**(8), **volutil**(8)

## AUTHOR

- David Steere, 1990, Created
