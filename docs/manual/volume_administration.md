# Volume Administration

## Concepts

Although users view the Coda file system as a hierarchy of directories and
files, system administrators view the Coda file system as a hierarchy of
volumes.  Each volume contains a subtree of related directories and files which
is a subtree of the entire file system.  Volumes, then, parallel traditional
Unix file systems.  Like a Unix file system, a volume can be mounted.  Thus,
the root of a volume can be named within another volume at a mount point.  The
Coda file system hierarchy is built in this manner and is then mounted by each
client using a conventional Unix mount point within the local file system.
Since all internal mount points are invisible, the user sees only a single
mount point for the entire Coda file system.

All system administration tasks are performed relative to a volume or a set of
volumes.  So, adding new users requires creating new volumes for their files
and directories;  quotas are enforced on volumes; and backups are performed on
a per-volume basis.  The volume abstraction greatly simplifies the
administration of large systems.

The main volume type that is used in typical Coda environments are the
_read-write, replicated volumes_.  Read-write, replicated volumes are logical
volumes which group together multiple read-write volumes.  Coda provides
protocols which allow read-write, replicated volumes to reside on a number of
servers and to be accessed even when some servers are inaccessible.

_Table 3: Coda volume types_

| Volume Type | Read from | Write to | Conflicts |
| ----------- | --------- | -------- | --------- |
| Read-Write Replicated | Any VSG Member | Any VSG Member | Yes |
| Backup | Only custodian | Nowhere | No |

## Creating a Volume

Typically, volumes consist of a single users data objects or other logically
connected groups of data objects.  Four factors should be used in dividing the
file system tree into volumes.

- The volume is the unit of quota enforcement and, potentially, accounting.
- The rename command is prohibited across volume boundaries.  Thus,
  manipulation of the tree structure at a granularity other than a whole volume
  (e.g. moving the mount point) or less than a volume (e.g. moving directories
  or files within a volume) is expensive.  Moving a subtree of a volume to
  another volume requires copying every byte of data in the subtree.
- The size of the volume should be small enough that moving volumes between
  partitions is a viable approach to balancing server disk utilization and
  server load.  Thus, volumes should be small relative to the partition size.
- Finally, the size of a volume must not exceed the capability of the backup
  media.

A volume naming convention should also be used by those administrators who
create volumes.  Volume names are restricted to _32_ characters and should be
chosen so that given a volume name, a system administrator (who knows the
naming conventions) can determine its correct location in the file system
hierarchy.  The convention used by the Coda project is to name volumes by their
function and location.  Thus, a replicated volume named _u.hbovik_ is mounted
in `/coda/usr/hbovik` and contains hboviks data.  A project volume is prefixed
by _p._ and a system volume is prefixed by _s._.  Similarly, volumes containing
machine specific object files are prefixed by the machine type.  For instance,
_p.c.alpha.pmax.bin_ contains project coda binaries for our current alpha
release and is mounted on `/coda/project/coda/alpha/pmax_mach/bin`.

Use the [createvol_rep](../manpages/createvol_rep.8.md) command to create
read-write replicated volumes.  This command is actually a script which
ultimately invoke the [volutil](../manpages/volutil.8.md) command with the
create option at the appropriate server.  The volume will contain an access
list initialized to _System:Administrators rlidwka_ and _System:AnyUser rl_.
Creating the volume does not mount the volume within the file system hierarchy.
Mounting the volume as well as changing the access list or the quota must be
done using the [cfs](../manpages/cfs.1.md) command from a client.  A new volume
may not be visible at client workstations for some time (see [Building the
volume databases](#building-the-volume-databases) below).

A few concrete examples should clarify the use of some of these commands. On
the SCM, `createvol u.hbovik mahler /vicepa` will create a non-replicated
volume named _u.hbovik_ on server _mahler_'s `/vicepa` partition. Similarly,
`createvol_rep u.hbovik E0000107 /vicepa` will create a replicated volume named
_user.hbovik_ on each server in the _Volume Server Group (VSG)_ identified by
_E0000107_.  The file `/vice/db/VSGDB` contains the mapping between VSGs and
their identifications. The names of the replicas will be
_user.hbovik.&lt;n&gt;_, where &lt;n&gt; is a number between _0_ and _|VSG| - 1_.

## Mounting a Volume

In order to use a volume which you have created and added to the appropriate
databases, you must mount the volume.  Although Unix file systems must be
mounted upon reboot in Unix, Coda volumes are mounted only once.  To mount a
Coda volume, you must be using a Coda client and be authenticated as a user
(use the **clog**) who has write access to the directory in which the mount
point will be created.

Mount the volumes using the command `cfs mkmount <filename> <volname>`. Note
that **cfs** creates &lt;filename&gt; automatically.  For example, `cfs mkmount
/coda/usr/hbovik u.hbovik` will create `/coda/usr/hbovik` and then mount the
_u.hbovik_ volume created in the example in
[Creating a Volume](#creating-a-volume).  The volume is now visible to all
users of the Coda file system.  More information about the **cfs** command can
be found in [Getting Started](getting_started.md) and in the
[cfs.1](../manpages/cfs.1.md) man page.

!!! warning

    When mounting a volume, avoid creating multiple mount points for it.  Coda
    cannot check for this.

## Deleting a Volume

When a volume is no longer needed, it may be purged by running the **purgevol**
or **purgevol_rep** scripts on the SCM.

Before removing a volume, you should probably create a backup for offline
storage (see
[Creating a Dump of a Replicated
Volume](#creating-a-dump-of-a-replicated-volume)).  The
volume's mount point should be removed with the **cfs** command (see the
_rmmount_ option) before purging the volume (if possible).  Note that purging
the volume will not purge related backup volumes.  Backup volumes should be
purged with the **purgevol** script or the **volutil purge** command.

## Dumping and Restoring a Volume

### Creating a Dump of a Replicated Volume

On the SCM, you need to clone the read-write copy of the volume.  You can use
the command `volutil clone <volumeid>`.  This command will create a read-only
volume with the name _binaries.readonly_ (assuming that your volume was called
_binaries_).  Next, you will need to dump this cloned volume to a file with the
command `volutil dump <volumeid> <filename>`.

The next section explains how to restore a dumped volume.

### Restoring Volume Dumps

For complete details on the backup/restore process, see [Backup](backup.md).
In short, one first needs to get the correct dumpfile, possibly merging
incremental dumps to an older full dump to get to the desired state to be
restored. Once this file is obtained, use the [volutil](../manpages/volutil.8.md)
restore facility.

    volutil restore <filename> <partition> [volname] [volid]

The &lt;partition&gt; should be one of the `/vicep?` partitions on the server.
You may optionally specify the volumeId and the volume name for the restored
volume. This is useful when creating read-only replicated volumes.  Note that
currently dump files are not independent of byte ordering -- so volumes cannot
be dumped across architectures that differ in this respect.

After the volume has been restored, the volume location databases have to be
updated before the clients can see the restored volume. This is explained in
[Volume Location Data Base](#volume-location-data-base).

## Building the Volume Databases

### Volume Location Data Base

The _Volume Location Data Base (VLDB)_, is used to provide volume addressing
information to workstations.  Copies of the VLDB reside on the servers and are
updated periodically.  The VLDB lists the latest known location or locations of
all on-line volumes in the system. A human readable version of the VLDB is
maintained on the SCM in the file `/vice/vol/AllVolumes`.

The VLDB is maintained on the SCM. When you wish to update it, run
[bldvldb.sh](../manpages/bldvldb.sh.8.md) from `/vice/bin/` on the SCM.  The
script gathers a copy of the `/vice/vol/VolumeList` file from all the servers,
merges it into a single list, and builds a new VLDB. The **updateclnt** /
**updatesrv** processes will then propagate the new VLDB to all the servers.

!!! note

    **createvol** and **purgevol** scripts automatically invoke **bldvldb.sh**.

### Volume Replication Data Base

The _Volume Replication Data Base (VRDB)_, is used to provide information about
replicated volumes to client workstations.  Copies of the VRDB reside on all
servers and are updated periodically.  The VRDB maps each logical volume to its
corresponding set of physical volumes.  A human readable version of the VRDB is
maintained on the SCM in the file `/vice/vol/VRList`.  The **makevrdb** option
to the [volutil](../manpages/volutil.8.md) command will create a new VRDB which
will automatically be distributed to the other servers.

### Volume Storage Group Data Base

The _Volume Storage Group Data Base (VSGDB)_, is created during setup process
and, currently, is maintained by hand.  Each valid volume storage group has an
entry in this data base containing an identification number and the names of
the servers in the group.

## Getting Volume Information

The **cfs** command provides information on volumes. **cfs** can only be used
on a machine which has a running **venus** (such as a client workstation).
**cfs** is described in [Getting Started](getting_started.md) as well as in the
[cfs.1](../manpages/cfs.1.md) manual page.
