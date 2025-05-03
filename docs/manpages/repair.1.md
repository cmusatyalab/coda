---
title: REPAIR(1)
footer: Coda Distributed File System
date: 2005-04-25
---

## NAME

repair - Repair conflicts in the Coda Distributed File System

## SYNOPSIS

**repair** \[-d]

**repair** -allowclear

**repair** object\_in\_conflict fixfile \[repair\_options]

## DESCRIPTION

The Coda repair tool allows you to manually resolve two kinds of
conflicts resulted from partitioned updates. Server-server conflicts are
conflicting mutations performed on partitioned servers that can not be
automatically resolved. Local-global conflicts are caused by mutations
performed on a disconnected client that are in conflict with the global
server state.

To use the repair tool interactively, type **repair** at the command
prompt.

Server-Server conflicts can be repaired from the command line without
entering interactive mode. This is useful if you need to repair many
conflicts within a volume at a time and wish to write a shell script.
Please see the **EXAMPLES** section for examples on invoking complete
repair sequences from the command line.

A description of the **repair** commands follows:

**beginrepair** *object*

:   verifies that *object* is indeed in conflict. It will print out
    messages to indicate whether the current repair session is for
    server-server conflict or local-global conflict.

For a server-server repair session, this command locks the corresponding
volume and and mounts its individual replicas read-only. It will inform
the users to only use the **comparedirs**, **dorepair** and
**removeinc** commands to repair the conflict.

For a local-global repair session, both local and global replicas of
*object* are visible at **object/local** (read-only) and
**object/global** (mutable and serving as the workspace for storing the
repair result for *object*). You need to iterate through the current
sessions **local-mutations-list** containing all the local updates to
*object* and its descendants. Each operation in **local-mutations-list**
must be accounted for and Venus maintains the **current-mutation** being
iterated. Use the **checklocal** command to find out the conflict
between the **current-mutation** and the server state. Note that not all
local mutations are necessarily in conflict, and you can use the
**listlocal** command to see all the operations in
**local-mutations-list**. You can advance the iteration to the next
operation using either the **preservelocal** or the **discardlocal**
command with the former replaying the **current-mutation** operation on
the relevant global replicas. To speed up the iteration, the
**preservealllocal** command repeats **preservelocal** until the
**local-mutations-list** is exhausted or the first replay failure.
Similarly, the **discardalllocal** command repeats **discardlocal**
until exhausting the **local-mutations-list**. You can use external
tools such as **emacs**(1) to make direct updates on needed replicas
under **object/global**. Use the **quit** command to either commit or
abort the session.

**quit**

:   If the current session is repairing a server-server conflict, this
    command releases all the volume-level locks and causes the repair
    tool to return to the shell. If the current session is repairing a
    local-global conflict, this command asks you whether to commit or
    abort the repair session. If you answer yes, the mutations performed
    on the relevant global replicas will be committed to the servers.
    Otherwise, the repair result will be aborted as if this repair
    session has never happened.

**help**

:   Prints out a help message.

Use the following commands to repair a server-server conflict:

**comparedirs** *object* *fixfile* \[**-acl** *user* *rwlidka*]
\[**-mode** *newmode*] \[**-owner** *username*]

:   Compares the mounted read-only replicas of a directory in conflict
    and prints the repair commands in fixfile to make the replicas
    identical. To print out the repair commands on your terminal give
    stdout as the pathname for fixfile. Compensating actions for
    Name/Name conflicts and Update/Update conflicts are not given. The
    user is only told about their existence and required to edit the
    fixfile manually. You should have already done a **beginrepair** on
    *object* and this command works only if *object* is a directory.

**dorepair** *object* *fixfile*

:   Does the actual repair of an object. If the repair succeeds, each
    accessible replica will be marked consistent. You will be prompted
    for the arguments if they are missing, and will be asked to confirm
    the repair. You should have already done a **beginrepair** on this
    object (or on on some other object in this volume.). If *object* is
    a file or symbolic link, *fixfile* must provide its new contents. If
    *object* is a directory, *fixfile* must provide a sequence of
    directory repair commands for each replica. The format of fixfile
    for directories is as follows:

    replica <servername> <id of replica1>
        <repair commands for replica1>
    replica <servername> <id of replica2>
        <repair commands for  replica2>
    and so on

Repair commands are given one per line. Blank lines are ok. **id of
replica1**, **id of replica2**, etc. are numbers that identify each
replica. These are the same as the volume ids of read-write volumes
corresponding to a replicated volume. The volume ids can be obtained by
doing an ls on the inconsistent object, after the **beginrepair**
command has succeeded. The directory repair commands are:

    createf <filename> <fid.0> <fid.1> <fid.2>
    creates <symlinkname> <fid.0> <fid.1> <fid.2>
    createl <linkname> <fid.0> <fid.1> <fid.2>
    created <dirname> <fid.0> <fid.1> <fid.2>
    removefsl  <filename or symlinkname or linkname>
    removed  <dirname>
    mv <srcname> <tgtname>
        <src <fid.0> <fid.1> <fid.2>>
        <target <fid.1> <fid.2>>
    setacl  <username> [rwlikda]
    delacl  <username>
    setmode <newmode>
    setowner <new owner name>
    setmtime <new modified time>

Note that for the **setacl** command, the short form access rights of
**all** and **none** can also be used.

**removeinc** *object*

:   Removes the inconsistent *object* if it is file or a symbolic link.
    If the object is a directory, all the descendants of the object will
    be removed in all the accessible replicas and the directory itself
    will be removed as long as its replicas are identical. If the owner
    or the ACL of the directory replicas are different, you have to
    repair the conflict first.

**clearinc** *object*

:   Compares the mounted read only replicas of a directory in conflict
    and if the replicas are identical it clears the inconsistency flag
    of the replicas. Otherwise it will inform you about the inequality
    of the replicas. You should run the **comparedirs** command to find
    out the cause of conflict. This command should be used only for
    directories. Files and symbolic links are cleared of their
    inconsistency with the **dorepair** command.

The following commands are used only for repairing local-global
conflicts:

**checklocal**

:   Checks to see if the **current-mutation** being iterated by the
    current local-global repair session is in conflict with the server
    state. It displays the operator and operand (s) of the
    **current-mutation** operation and indicates whether it is in
    conflict with the relevant server replicas. If it is in conflict, a
    brief reason of the conflict is given. Note that this command does
    not advance the iteration of the **local-mutations-list**.

**discardlocal**

:   Simply advances the iteration of the **local-mutations-list** of the
    current local-global repair session to the next operation. Use this
    command when the user does not want the **current-mutation**
    operation to have any effect on the repair result.

**preservelocal**

:   Tries to replay the **current-mutation** of the current local-global
    repair session on the relevant global replicas. In other words, it
    will try to preserve the effect of the **current mutation** in the
    repair result. If the replay succeeds, the iteration of
    **local-mutations-list** will be advanced to the next operation. The
    effect of the replay is visible only on this client and not on the
    server until the repair result is successfully committed. If the
    replay fails, information about the reason of the failure will be
    displayed.

**discardalllocal**

:   Repeatedly performs the **discardlocal** command until the
    **local-mutations-list** is exhausted. Its effect is to finish the
    iteration and discard the effect of all the remaining mutations on
    the repair result.

**preservealllocal**

:   Repeatedly performs the **preservelocal** command until the first
    failure or the iteration of **local-mutations-list** is exhausted.
    This command is used if the user wants to preserve the effect of all
    the remaining mutation operations in the repair result.

**listlocal**

:   Prints out all the mutation operations in the
    **local-mutations-list** of the current local-global repair session.

The following commands existed in old versions but are no longer
supported:

**showreplicas** *object*

:   Shows the names of the individual replicas of *object*, and the
    pathnames by which these replicas may be examined read-only. A
    **beginrepair** must have been done earlier on this object (or on
    another object in the same volume).

**unlockvolume** *pathname*

:   Tells Venus to unlock the specified volume in repair. No check is
    done to see if you locked the volume during this repair session. The
    primary use of this command is to unlock volumes that were locked
    during a previous, aborted, invocation of the repair tool. The
    command will fail if Venus discovers that you do not hold the repair
    lock on the server. This could happen, for example, if your aborted
    repair occurred on another workstation, or if you were not repairing
    the volume in the first place.

## EXAMPLES

This will cause repair to examine the object "common", generate a fix
file for it and in addition to the contents of the fix file, and set the
acl for hmpierce to **rlidwka** on all replicas.

``` sh
repair common /tmp/fix -acl hmpierce all
```

The same repair would look like this in interactive mode:

    repair> beginrepair common
    repair> comparedirs common /tmp/fix -acl hmpierce all
    repair> dorepair common /tmp/fix
    repair> endrepair
    repair> quit

## AUTHORS

* M.\ Satyanarayanan, 1989, Created

* Puneet Kumar, 1991, Substantially revised

* Joshua Raiff, 1993, Created man page

* Qi Lu, 1995, Added local-global repair commands and revised man page

* Henry M.\ Pierce, 1998, updated for command line options
