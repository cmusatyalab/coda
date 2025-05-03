---
title: CREATEVOL_REP(8)
footer: Coda Distributed File System
---

## NAME

createvol\_rep - create read-write replicated volume

## SYNOPSIS

**createvol\_rep** volume-name server \[partition] \[server \[partition] ...] \[volume-id]

## DESCRIPTION

**createvol\_rep** is a front end to *volutil create\_rep* and is used
to create a Coda read/write replicated volume. The invoker must specify
the volume name (*volume-name*), and the server(s) on which the
volume should be created. (*server\[partition]*), the
partition name is required when the server manages more than one data
partition. Finally an replicated *volume-id* can be specified.

**createvol\_rep** first checks in `/vice/vol/AllVolumes` and
`/vice/vol/VRList` to see if the volume name already exists. If not, it
uses the `volutil create_rep` command to create the volume at each of
the replication sites. It then rebuilds the Volume Location Data Base
(VLDB) and the Volume Replication Data Base (VRDB).

The optional replicated *volume-id* specifies the *replicated* volumeid of the
volume being created. By default, the volume id in `/vice/vol/maxgroupid` is
used. Each time it is used it is also updated by adding 1 to it.

After the replicas are created at each replication site, a
new VLDB is built automatically using **bldvldb.sh**(8), and the Volume
Replication List in `/vice/vol/VRList` is updated. The VRList contains one
line for each replicated volume. Each line specifies the replicated volume
name, replicated volume id, number of replication sites, and the local volume
id at each replication site. This file is now used to create a new Volume
Replication Data Base (VRDB) using the `volutil makevrdb /vice/vol/VRList`
command.

## EXAMPLES

To create a replicated volume "coda.rep" on 3 sites foo, bar and gorp
use:

    createvol_rep coda.rep foo bar gorp

To assign a predetermined replicated volume id, use

    createvol_rep coda.rep foo bar gorp 7F000003

where "7F000003" is the replicated volume id.

## DIAGNOSTICS

This command must be issued at the System Control Machine (SCM). Also,
it must be invoked with effective user id of root. This command does not
check for return codes from the `volutil create_rep` command. The
invoker must check `/vice/vol/VRList` and `/vice/vol/VolumeList` at each
replication site to see if the volume was created.

## FILES

- `/vice/vol/VRList` - contains information on replicated volumes
- `/vice/vol/VolumeList` - name of volume created at each site is appended to it
- `/vice/db/VRDB` - is used to describe the replicated volumes in terms of its
  non-replicated members.
- `/vice/vol/AllVolumes` - is used to check if volume exists
- `/vice/vol/maxgroupid` - is used to assign a volume id to the replicated volume

## SEE ALSO

**bldvldb.sh**(8), **volutil**(8)

## AUTHOR

- Puneet Kumar, 1990, Created
