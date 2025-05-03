---
title: HOARD(1)
footer: Coda Distributed File System
date: 2005-04-25
---

## NAME

hoard - Hoard database front-end

## SYNOPSIS

**hoard** \[-d] \[-f source | cmds]

## DESCRIPTION

**hoard** is a front-end to the **hoard** database (HDB) managed by the
Coda cache manager, **Venus**. The HDB provides a means for users to
explicitly control Venus caching behavior. HDB entries specify the
degree of a users interest in particular file system objects. Venus
combines this information with implicit knowledge that it has about file
access patterns to try to keep the "best" set of objects in its cache
at all times. The HDB is maintained in non-volatile storage, so it
survives Venus restarts and client reboots.

Users manipulate the HDB by issuing commands to the hoard program. The
following hoard commands are currently recognized:

**add** *filename* *attributes*

**clear** *uid*

**delete** *filename*

**list** *outfile* *uid*

**modify** *filename* *attributes*

Hoard distinguishes between **children** of a directory, which are
members of the directory, **descendants** which are either children or
descendants of children of the directory.

Commands may be abbreviated by their first letter. Parameters in angle
brackets are optional, and have default values if unspecified. The
**attributes** parameter is specified as a string of options separated
by : characters. The currently recognized options are:

**1-1000**

:   Assign this object the hoard **priority** indicated.

**c**

:   Current **children** of this directory will inherit its hoard
    status.

**c+**

:   Current and future **children** of this directory will inherit its
    hoard status.

**d**

:   Current **descendents** of this directory will inherit its hoard
    status.

**d+**

:   Current and future **descendents** of this directory will inherit
    its hoard status

If the **uid** in the clear and list commands is unspecified, the
entries of all users are cleared or listed respectively. The default
hoard **priority** is 10.

**EXAMPLE HOARD COMMAND FILE**

    clear
    add /coda/project/coda/src 100:d+
    add /coda/usr/jjk/.login   1000

Access to the hoard database is restricted in the following ways. All
hoard commands fail unless the hoard program is executed by the user who
is identified as the **primary user** in the venus.conf configuration
file. If no primary user is specified, special permission is given to
the console user who is logged in on the local (or first virtual)
console. Primary users may add entries and access existing entries
without restriction. Other users may not add hoard entries, and they may
only clear, delete, list, or modify their own entries.

The command-line options are:

**-d**

:   Enables debugging output.

**-f**

:   Take commands from *source*. -f must be the last argument if
    specified. An argument of - means, use **stdin** as the source file.
    Source statements may be given directly on the command line (one per
    line) by enclosing them in single quotes.

## DIAGNOSTICS

Hoard copies command lines that it cannot parse to **stderr**. If a
syntactically correct command is rejected by Venus, the corresponding
pioctl, its arguments, and the errno are copied to **stderr**.

## BUGS

- Negative priorities should be allowed.

## SEE ALSO

**venus**(8)

## AUTHORS

- Jay Kistler, 1990, Created
