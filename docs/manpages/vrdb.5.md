---
title: VDRB(5)
footer: Coda Distributed File System
---

## NAME

vrdb - Volume Replication Data Base specification

## DESCRIPTION

The volume replication data base is stored in binary form in
`/vice/db/VRDB` on each file server. The *makevrdb* option of the
**volutil**(8) program constructs the VRDB on the system control machine
(SCM).

The data base consists of fixed-length records, each of which describes
a replicated (aka group) volume. Each file server copies the VRDB into
memory at start-up and whenever an updated version of it is received.
The data base is used to map group volume names and numbers into a VSG
and the set of read-write volumes which comprise it.

The VRDB is generated from an ASCII version stored on the SCM in
`/vice/vol/VRList`. The VRList is updated as a side-effect of every
create and purge of a replicated volume. Its format is:

    <group volname> <group volnum> <# of replicas> <rwvol 1> ... <rwvol 8> <VSG num>

A sample line from the VRList is:

    project.coda.src 7f000010 3 c9000012 ca000013 cb000013 0 0 0 0 0 E0000107

Note that all volume and VSG numbers are given in hex. Details of the
VRDB structure can be found in `vrdb.h`.

## FILES

- `/vice/db/VRDB`
- `/vice/vol/VRList`

## BUGS

File servers keep the in-memory copy as a singly-linked list. It should
be converted to a pair of hash-tables, one keyed by group volname, the
other by group volnum, for fast lookup.

The maximum number of replication sites is fixed at 8. Adding, deleting,
or moving replication sites after creation is not supported.

## SEE ALSO

**volutil**(8), **maxgroupid**(5), **vsgdb**(5)

## AUTHOR

- Jay Kistler, 1990, created
