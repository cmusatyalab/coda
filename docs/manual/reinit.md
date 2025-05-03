---
title: Reinitializing after Disaster
---
# Reinitializing Coda Servers after a Disaster

As Coda is still an experimental system, the state of a Coda File System may
sometimes get to the point where you will want to reinitialize all of the
servers.

!!! note

    This is a fairly drastic step and should be treated as a last resort.  It
    will typically take an entire day or more to perform and the system will be
    unavailable during that time. You will *not* be able to reintegrate after a
    re-initialization.

This chapter explains the steps necessary to reinitialize your system and
restore your data on "clean" servers.  The basic steps are as follows:

 1. Obtain backup dump files for all of the volumes in the system.
 1. Reinitialize RVM on the servers.
 1. Recreate the volumes that existed before the re-initialization.
 1. Restore and mount the backup clones created in step 1.
 1. Mount the volumes created in step 3.
 1. Copy the data from the restored volumes to the newly created volumes.
 1. Unmount the new volumes and backup clones.
 1. Flush client caches.
 1. Test the system.

## Obtaining Backup Dump Files

If the system is in a state that backups can be successfully run, we suggest
running the **backup.sh** script to capture the most recent state.  You may,
however, use the tapes from your last successful backup and skip this step.
See [Backup System](backup.md) and **backup**(8) for
more details on creating backups.  You will need to create new `dumplist` files
for the backups.  If you include the volumes that you do not normally back up
in the newly created dumplists, you will be able to restore those volumes
rather than re-create their contents.  See **dumplist**(5)
for details on the format of these files.  To create the dumplists, use the
following steps:

 1. Use `/vice/vol/VolumeList` and `/vice/vol/VRList` to create a list of all
    of the volumes on the servers you want to reinitialize.
 1. Sort the volumes by size.  Use the `cfs lv` command to get an estimate on
    the size that each volume will take.  Remember that if a volume is
    replicated, each replica will need space on the tape.
 1. Divide the volumes into groups that will fit onto a single backup tape (at
    CMU this is about 4 GB)
 1. Run `backup` on the dumplist files.

## Reinitializing RVM on the Servers

Use **rvmutl** and **rdsinit** to initialize RVM.  Unless you are
reinitializing in order to change the size of the RVM heap, be sure to use the
same sizes that you used previously.  See
[RVM Initialization](server_installation.md#rvm-initialization) for the details
on RVM initialization process.

## Recreate the Backed Up Volumes

Use [createvol_rep](../manpages/createvol_rep.8.md) to create all of the volumes
that you backed up in [Obtaining Backup Dump Files](#obtaining-backup-dump-files).

## Restore the Backup Clones

First, restore the read only backup of the `coda_root` volume. You should have
created the new `coda_root` volume in the previous step. This will
automatically be mounted.  Next create a directory `/coda/.../tmp/restore` and use
`volutil restore` to restore each of the backup clones to `/coda/.../tmp/restore`.
See the [volutil.8](../manpages/volutil.8.md) man page for more details on
restoring the volumes.

## Mount the Recreated Volumes

Create a directory `/coda/.../tmp/inited` and mount all of the volumes created
earlier in [Restore the Backup Clones](#restore-the-backup-clones) in this
directory.

## Populate the New Volumes

Run the script **restorevolumes**(8) to restore all of the volumes that were
backed up in [Obtaining Backup Dump Files](#obtaining-backup-dump-files).

## Unmount the Backup Volumes

Use `cfs rmmount` to unmount all of the volumes in `/coda/tmp/restored` and
`/coda/tmp/inited`.  Then remove these directories.  See the man page
[cfs.1](../manpages/cfs.1.md) for more details on unmounting volumes.

## Flush Client Caches

You need to flush the client caches as they will not have the correct volume
information anymore.  Use the `cfs flushcache` command to do this.

## Test the system

After you've flushed the client caches the system is ready to run.  Do some
testing to make sure Coda is functioning correctly and allow the users back
into the system.
