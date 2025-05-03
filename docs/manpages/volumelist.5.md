---
title: VOLUMELIST(5)
footer: Coda Distributed File System
---

## NAME

volumelist - volumelist file specification

## DESCRIPTION

Every server keeps a list of volumes for which it is the custodian. This
list is in `/vice/vol/VolumeList`. Every time a volume is created, an
entry corresponding to that volume is made in this list. **bldvldb.sh**(8)
uses this list to generate the Volume Location Data Base (VLDB).

The first few lines of this file contain information pertaining to the disk
partitions. These lines have the following format:

    P[partition-name] H[hostname] T[total usable space on this partition] F[free space on this partition]

There is one line entry for each volume on the server.  Each line begins with
W, R or B depending on whether the volume is read-Write, Readonly or Backup.
The format is as follows:

    R│W│B[volume-name] I[volume-id] H[server id] P[partition name] m[min quota] M[max quota] U[disk space used] W[parent id] C[creation date] D[copy date] B[backup date] A[volume usage]

## FILES

- `/vice/vol/VolumeList`

## SEE ALSO

**createvol**(8), **createvol\_rep**(8), **volutil**(8), **bldvldb.sh**(8)

## AUTHOR

- Puneet Kumar, 1990, created
