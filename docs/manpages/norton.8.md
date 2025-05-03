---
title: NORTON(8)
footer: Coda Distributed File System
date: 2005-04-25
---

## NAME

norton - Coda File System RVM debugger

## SYNOPSIS

**norton** RVM\_log RVM\_data RVM\_data\_length

## DESCRIPTION

**norton** is a utility program that allows you to display Coda file
server structures that are stored in **RVM**. Eventually, **norton**
will be a full special purpose debugger that allows you to manipulate
the structures as well. **norton**s command interface uses the gnu
**readline** library which features Emacs style command editting as well
as maintaining a command history. Command completion is also supported
by using ++tab++ and ++esc++-?.

The available commands are:

**show directory** *volid* *vnum* *uniqifier*

:   Lists the contents of the directory indicated by
    *volid.vnum.uniqifier*. Each entries vnode number and uniquifier are
    also listed.

**delete name** *volid* *vnum* *uniqifier* *file* *flag*

:   Remove *file* from the directory specified by
    *volid.vnode.uniqifier*. If flag is nonzero the linkcount of the
    directory is reduced by one.

**NOTE: delete name does not do anything to the vnodes associated with**
*file*, **you must remove the vnodes by hand or update their link
count.**

**delete volume** *volid*

:   Mark a volume with a "destroyme" flag, so that the salvager will
    destroy it on the next server startup.

**rename volume** *name* | *volid* *newname*

:   Rename a volume replica.

**examine** *addr* *len*

:   Print *len* bytes starting from *addr* in hex and ascii.

**list volumes**

:   Display a list of all the volumes on the server. This list includes
    the volume index, name, number, and type.

**show free large | small**

:   Display all of the vnodes on either the large or small free vnode
    list.

**show heap**

:   Display RVM heap usage.

**show index** *volname* | *volid*

:   Display the RVM index of the given volume name or number.

**show vnode** *volid* *vnode* | ? *uniqifier*

:   Show the specified vnode. If a **?** is given rather than a vnode
    number, all of the volumes vnode lists are searched for a vnode with
    a matching uniquifier.

**show volume** *volname* | *volid*

:   Show a summary of the specified volume.

**show volumedetails** *volname* | *volid*

:   Show all of the RVM state of the specified volume.

**create name** *parent\_volid* *parent\_vnode* *parent\_uniqifier* *name*
*child\_vnode* *child\_uniqifier*

:   Insert a vnode in a directory. The parent fid gives the directory in
    which name is to be inserted. The child fid refers to the child
    vnode. The link count of the directory is increased if the child is
    a directory vnode.

**x** *addr* *len*

:   Print *len* bytes starting from *addr* in hex and ascii. An alias
    for **examine**.

## AUTHORS

- Joshua Raiff, 1995, Created
- Peter Braam, 1997, new features
