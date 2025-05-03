---
title: CODAREADDUMP(8)
footer: Coda Distributed File System
---

## NAME

codareaddump - Utility to examine dumpfiles created by the Coda backup facility

## SYNOPSIS

**codareaddump** \[dumpfile]

## DESCRIPTION

The **codareaddump** program is an interactive facility to allow the
user to get information from a dump file produced by the Coda backup
mechanism. For now it is simply a way of looking at the information in
a dump file. One cannot use it to modify the dump file.

The **codareaddump** program supports the following commands:

**openDumpFile** *dumpfile*

**showHeader**

**showVolumeDiskData**

**setIndex** *index_type*

**nextVnode**

**skipVnodes** *nVnodes*

**quit**

**codareaddump** uses the *ci* command interface, which allows the use of
unique prefixes for commands. Unspecified parameters will be prompted
for, and default values can be used. The **showHeader** command prints out
the header of the dump, which is separate from the header of the volume
that was dumped. The **showVolumeDiskData** prints out the header of the
volume.

The dump contains two streams of vnodes, one for directories and one for
files and symbolic links. The **setIndex** command specifies to
**codareaddump** which stream you wish to examine. Once an index has
been specified, the **nextVnode** command displays the next Vnode
(object) in the stream. Note that movement through the stream is
one-directional. To revisit a Vnode, use the **setIndex** command to
reset the program back to the beginning of the stream. One can jump
through the index by use of the **skipVnodes** command, which takes the
number of vnodes to skip, and reads past (without displaying) that many
vnodes in the stream.

## Future Work

Usage of this facility should suggest more commands to help
administrators parse dumpfiles. In particular, a command to seek ahead
to a specified offset in the dump would be useful.

## SEE ALSO

**codamergedump**(8), **backup**(8), **volutil**(8), Coda Manual

## AUTHOR

- David C.\ Steere, 1991, Created
