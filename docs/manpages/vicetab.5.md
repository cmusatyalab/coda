---
title: VICETAB(5)
footer: Coda Distributed File System
date: 2005-04-25
---

## NAME

vicetab - information about Coda server data partitions

## DESCRIPTION

**vicetab** is a table of Coda data partitions found on individual
servers comprising a Coda network server. This includes partition(s)
used by the backup coordinator to store dump files to placed on a
suitable backup media. This file must be shared among all machines
comprising a Coda hub so edits should be done only on the designated
SCM.

**EXAMPLE VICETAB FILE**

    tye             /vicepa         ftree  depth=5,width=8
    tye             /vicepb         ftree  width=8,depth=5
    taverner        /vicepa         ftree  width=8,depth=5
    taverner        /usr/vicepb     ftree  depth=4,width=8
    tallis          /vicepa         ftree  width=8,depth=5
    tallis          /vicepb         ftree  width=8,depth=5
    dvorak          /backup1        backup
    dvorak          /backup2        backup
    dvorak          /backup3        backup

Where column 1 specifies the server as returned by the system call
gethostbyname().

Column 2 specifies the directory of the Coda data tree which must be a
local file system partition for optimal performance. **NOTE: if a server
serves than more than one Coda data partition, each data partition must
have a seperate entry in vicetab**.

Column 3 specifies the Coda partition type.

**ftree** an "inode number" directory and file naming system for storing the
Coda filesystem into a local file system tree structure: e.g. inode 4711 in
base 10 with width 1 and depth 5 would become 0/4/7/1/1. So, using the above
example, a width of 4 and a depth of 2 would make the file name 4/711. If this
storage type is specified, the width and depth must also be given in the 4th
column separated by a comma.

Column 4 specifies the width and depth of the ftree.

**width** the maximum "width" of an ftree data partition.

**depth** the maximum "depth" of an ftree data partition.

In the case of the backup coordinator, only the first three columns are
used, with a partition type of **backup** specified in the third column.

This file should only be edited on the designated SCM machine and then
allowed to propagate.

## BUGS

None we currently are aware of.

## AUTHOR

- Henry M.\ Pierce, 1997, created
