---
title: VOLUTIL(8)
footer: Coda Distributed File System
---

## NAME

volutil - volume utility subsystem

## SYNOPSIS

**volutil** \[-h server] \[-t timeout] \[-d debuglevel] command parameters ...

## DESCRIPTION

**volutil** is a RPC interface to the volume utility subsystem of the
file server. The volume utilities are used to perform administrative
functions like creating, cloning, purging, dumping and restoring
volumes. Each of these functions can be invoked via the *command*
parameter. Each command has a set of parameters that it expects. These
are listed below along with a short description of each command. The
**volutil** utility may be instructed to perform the operations on a server
at a remote site by specifying the server to which to connect with the
**-h** option. The default is to connect to a server on the local
machine.  The **-t** option may be used to specify the timeout (in
seconds) to be used by RPC2.

**ancient** *group-id* *rep-id*

:   Tell the server that backup succeeded for this volume. The next dump of
    this volume, if incremental, will be based on the state represented by
    this backup. The input should be in Hex.

**backup** *volume-id*

:   Create a backup clone of a read/write volume. If a backup clone already
    exists, update it to reflect the current state of the read/write volume;
    Otherwise, create a new read-only volume. The read/write volume must be
    locked for this to succeed. Backup unlocks the volume as a side effect.

**clone** *volume-id* \[ -n *new-volume-name* \]

:   Create a read only clone of a read write volume with (replica) ID
    (*volume-id*). The vnodes are actually copied but the inodes are
    marked copy-on-write i.e. inodes need to be copied only if modified. The
    name of the new cloned volume can be optionally specified by the
    *new-volume-name* parameter. Default value is "*volume-name.readonly*".

**create** *partition-path* *volume-name*

:   Create a nonreplicated read-write volume named *volume-name* on
    partition named *partition-path*. The **createvol**(8) command is a
    simplified front-end for this option.

**create_rep** *partition-path* *volume-name* *group-ID*

:   Create a replicated read-write volume named *volume-name* on
    partition named *partition-path*. The *group-ID* parameter is
    used to specify the ID of the replicated volume to which this replica
    will belong. The **createvol_rep**(8) command will also create a
    replicated volume.

**dump** \[-i\] *volume-id* *file-name*

:   Dump the entire contents of a volume (*volume-ID* in Hex) to a file
    (*file-name*). If the **-i** flag is used, the dump will be
    incremental, it will only include vnodes which have been modified
    since the last dump was taken. The dump is not machine independent,
    certain fields in the Vnode are not translated to network order.
    However, dump files can be used to create restored volumes on
    machines with a similar byte-order.

**dumpmem** *file-name* *address* *size*

:   Dump *size* bytes starting at *address* into *file-name*.

**elapse** on | off resolution | cb | mond | volDump \[ MultiRPC \]

:   Turn on or off timers for the given subsystem.

**info** \[-all\] *volume-name* | *volume-id* *file-name*

:   Print in ascii the contents of a volume into a file (*file-name*).
    The volume can be specified by its name, or by the volume-ID, specified
    in Hex. If **-all** is specified, contents of both large and small
    vnodes in that volume are also printed.

**lock** *volume-id*

:   Lock a volume to prevent write access to the volume. To other users it
    will appear as if the operation would time out.

**lookup** *volume-name* | *volume-id* *file-name*

:   Print information about a volume (specified by *volume-name* or
    *volume-id* specified in Hex) in *file-name*. Only meta-information,
    such as replicated group-ID, location, etc. is printed.

**makevldb** *volumelist*

:   Create a new Volume Location Database (**VLDB**). *volumelist* names a
    file containing volume parameters for all volumes in the system. This
    command typically is run on the system control machine. See also
    **bldvldb.sh**(8) and **volumelist**(5).

**makevrdb** *vrlist*

:   Create a new Volume Replication Data Base (**VRDB**). *vrlist* is a
    file containing entries describing replicated volumes. Each entry
    contains the name, group-ID, read-write ids, and the VSG address of a
    replicated volume. There is one entry per replicated volume in the
    system. See also **vrdb**(5).

**purge** *volume-id* *volume-name*

:   Delete a volume. For replicated volumes **purge** must be called from
    each server individually on the replicas at the different servers.
    (See **purgevol_rep**(8))

**restore** *file-name* *partition-path* \[*volume-name* \[*volume-ID*\]\]

:   Create a new volume on the partition named by *partition-path* and
    read in the contents from a dump in file *file-name*. The new volume
    will be given the name and ID specified on the command line. If either
    is unspecified, or if the volume ID is of illegal form, the server will
    allocate the ID or name based on internal rules. The volume-id should be
    specified in Hex.

**rvmsize** *volume-id*

:   Print the RVM statistics for the volume *volume-id*.

**setdebug** *level*

:   Set the debug level for the volume and directory packages to *level*. To
    reset it use zero for the *level* argument. The rpc2 debug level is set
    to *level* / 10.

**setlogparms** *volume-id* \[reson 4│0\] \[logsize *size*\]

:   Turn on resolution or change the log size for a volume. The volume ID
    can be either the replicated ID or the non-replicated ID. Resolution is
    turned on by specifying 4 after **reson** and can be turned off by
    specifying 0. The size of the log can also be changed for the volume.
    The **logsize** parameter refers to the number of maximum entries in
    the log.  This should be a multiple of 32. Typically this is set to
    8192.

**NOTE:** You should in all normal cases keep the resolution turned
**on**, and the argument for **reson** is in that case **4**!

**setvv** *volume-id* *vnode-number* *uniquifier* *version_numbers_(8)* *host_unique_flags*

:   Set the version vector for a vnode (with fid = *volumeId.vnodeNumber.uniquifier*).
    The new version vector is specified by the *version_numbers* and the
    *host_unique_flags* triple.

**showvnode** *volume-id* *vnode-number* *uniquifier* *file-name*

:   Print the contents of a vnode (with fid = *volumeId.vnodeNumber.uniquifier*)
    into *file-name*.

**shutdown**

:   Bring all volumes offline and bring down the server.

**swaplog**

:   Save the fileserver log output (in `SrvLog.old`) and start a new SrvLog.

**timing** on\|off *file-name*

:   Turn timing on or off.

**truncatervmlog**

:   Forcibly truncate the RVM log.

**unlock** *volume-id*

:   Unlock a volume locked via the `volutil lock` command.
    *volume-id* should be in hex.

**updatedb**

:   Make the file server read in a new VLDB and VRDB. The server assumes the
    databases to exist in `/vice/db/VLDB` and `/vice/db/VRDB`. This utility
    is useful for reading in new databases at non-scm machines.

## DIAGNOSTICS

This command must be run as root. It works only on the machine running
the server. Also, the token file `/vice/db/volutil.tk` must be
accessible.

## BUGS

The salvage option to **volutil** doesn't work right. Please don't try it.

## FILES

- `/vice/db/VSGDB`
- `/vice/file/SrvLog`
- `/vice/db/VLDB`
- `/vice/db/VRDB`
- `/vice/vol/VRList`

## SEE ALSO

**vrdb**(5), **volumelist**(5), **bldvldb.sh**(8), **createvol**(8),
**createvol\_rep**(8), **purgevol**(8), **purgevol\_rep**(8)

## AUTHOR

- Puneet Kumar, David Steere, 1990, Created
