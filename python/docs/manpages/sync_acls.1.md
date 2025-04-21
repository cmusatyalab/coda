---
title: CODA-SYNC-ACLS(1)
footer: Coda Distributed File System
---

## NAME

coda-sync-acls - Copy Coda ACLs from a source directory tree or a serialized
CodaACLs.yaml file

## SYNOPSIS

**coda-sync-acls** [-n] [-q] [-v] [-x] [\-\-write-aclfile CodaACLs.yaml]
[-d DESTINATION] SOURCE

## DESCRIPTION

**coda-sync-acls** is a tool to synchronize Coda ACLs from a source directory
tree or a serialized CodaACLs.yaml file. It depends on the `cfs` executable
from Coda to set the actual ACLs.

## OPTIONS

-n, \-\-dry-run

:   Do not perform any actions, just print what would be done.

-q, \-\-quiet

:   Be quiet, only display error messages.

-v, \-\-verbose

:   Be extra verbose.

-x, \-\-cross-volume

:   Traverse into other Coda volumes.

\-\-write-aclfile CodaACLs.yaml

:   Write collected acls to a new CodaACLs.yaml file.

-o, -d, \-\-destination DESTINATION

:   Destination where ACLs should be updated.
