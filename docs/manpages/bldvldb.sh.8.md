---
title: BLDVLDB.SH(8)
footer: Coda Distributed File System
---

## NAME

bldvldb.sh - build a new Volume Location Data Base (VLDB)

## SYNOPSIS

**bldvldb.sh**

## DESCRIPTION

**bldvldb.sh** builds a new volume location data base **VLDB**. It uses
the `/vice/db/hosts` file to get a list of servers to use to retrieve
the current list of volumes. It gets the list from the
`/vice/vol/VolumeList` file on each server. It combines the list into
`/vice/vol/BigVolumeList` and passes the combined list as a parameter to
the **volutil makevldb** command. This command produces a new **VLDB**
in `/vice/db` and updates the files *AllVolumes* and *partitions* in
`/vice/vol`. You must have root privileges to run this command.

## DIAGNOSTICS

This command can only be issued on the System Control Machine (SCM).
Bldvldb uses two mechanisms to get the VolumeList files from the various
servers in `/vice/db/hosts`. The first is ftp. In order for this to
succeed, there has to be a file called ".anonr" in the directory
`/vice/vol`, which is globally readable and contains a line with the
word "VolumeList" in it. If **bldvldb.sh** is unable to get the file
via ftp, it will attempt to use the CMU RFS remote file system. If
neither mechanism works, **bldvldb.sh** will skip over that servers
volumes when building the **VLDB**. The databases are propagated via the
update protocol. This takes up to five minutes. Attempts to access a new
volume from a client, prior to the propagation of all the databases to
all servers, may fail.

## FILES

- `/vice/vol/AllVolumes` - is created
- `/vice/db/VLDB` - is created
- `/vice/vol/VolumeList` - on each server is used to build the VLDB
- `/vice/db/hosts` - is a list of the active servers
- `/vice/vol/BigVolumeList` - is the result of combining `/vice/vol/VolumeList`
  from each server

## SEE ALSO

**update**(8), **volutil**(8)

## AUTHOR

- Puneet Kumar, 1990, Created
