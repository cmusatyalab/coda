---
title: MAXGROUPID(5)
footer: Coda Distributed File System
---

## NAME

maxgroupid - replicated volume number allocation mechanism

## DESCRIPTION

The system control machine (SCM) has a file, `/vice/vol/maxgroupid`,
which contains the maximum replicated (aka group) volume number that has
ever been allocated in the system. This is used as a simple way to
guarantee that group volume numbers are unique.

Replicated (or group) volume ids are allocated out of the same namespace as
replica ids and non-replicated volumes ids. The latter two types of ids have
a 1 byte (8 bit) prefix to identify the server on which they are stored. We
suggest using prefixes in the range 00-7F for replicated volume ids, and
prefixes in the range 80-FF for other volumes. When initializing a system,
put the number you wish to use for the first replicated volume in
`/vice/vol/maxgroupid`. For example, if you wish to use 7f000000 as the first
replicated volume, create `/vice/vol/maxgroupid` with the number 2130706432.

## FILES

- `/vice/vol/maxgroupid` on the SCM

## BUGS

This file should be salvaged, but it is not.

Removing this file (without reinitializing the whole system) is a recipe for
disaster. Grave confusion will result if group (or any other) volumes are
created with the same number.

## SEE ALSO

**servers**(5), **createvol\_rep**(8)

## AUTHOR

- Jay Kistler, 1990, Created
