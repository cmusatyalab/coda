# Common Scenarios

There are several common scenarios that you may encounter.  This chapter
attempts to list as many of them as possible and suggest how to handle each
scenario.

## Constructing a hoard file

Coda allows you to give files priorities that affect the cache manager.  The
higher the priority, the lower the possibility that a file will get flushed
from the cache to make space for a another file.  These priorities are stored
in a hoard database internal to **venus**.  This database is preserved across
invocations of **venus**, but will be erased when **venus** is re-initialized.

The best way to set up a hoard database is by creating hoard files.
After you've created the files once, you do not need to do it again for that
set of files.  You can create a hoard file by hand or by using the
[spy.1](../manpages/spy.1.md) program.  See the
[hoard.1](../manpages/hoard.1.md) man page for more details.

To run **spy**:

1. Run **spy** in the background, redirecting its output to a file.
1. Run all of the programs and access files you want to hoard.
1. Send a SIGTERM to **spy**.
1. Sort the output file, removing duplicates.
1. Remove unnecessary entries.
1. On each line add `a` at the beginning and a priority at the end

The following is an example of creating a hoard file for gnu-emacs. Note that
while running gnu-emacs, I explicitly enter _scribe mode_. This makes sure that
any scribe-mode specific files are fetched.

!!! example

    ``` sh
    $ spy > gemacs.out &
    [1] 316
    $ gnu-emacs
    $ kill %1
    $
    [1]    Done                 spy > gemacs.out
    $ sort -u gemacs.out > gemacs.hdb
    $ cat gemacs.hdb
    /coda
    /coda/i386_mach/omega/usr/local/emacs
    /coda/i386_mach/omega/usr/misc/.gnu-emacs
    /coda/misc/gnu-emacs/i386_mach/omega/bin/gnu-emacs
    /coda/misc/gnu-emacs/i386_mach/omega/lisp/scribe.elc
    /coda/misc/gnu-emacs/i386_mach/omega/lisp/term/x-win.el
    /coda/misc/gnu-emacs/i386_mach/omega/lisp/x-mouse.elc
    /coda/usr
    ```

Next you should delete the first and last line of the file as you do not need
them.  Then add the **hoard** specific commands.  The final file looks like:

    a /coda/i386_mach/omega/usr/local/emacs 600
    a /coda/i386_mach/omega/usr/misc/.gnu-emacs 600
    a /coda/misc/gnu-emacs/i386_mach/omega/bin/gnu-emacs 600
    a /coda/misc/gnu-emacs/i386_mach/omega/lisp/scribe.elc 600
    a /coda/misc/gnu-emacs/i386_mach/omega/lisp/term/x-win.el 600
    a /coda/misc/gnu-emacs/i386_mach/omega/lisp/x-mouse.elc 600

The `a` that starts each line tells hoard to **add** the named file to the
database.  The `600` that ends each line sets the file's priority. You may also
specify additional attributes for each line.  These attributes are separated
from the priority by a `:` and are:

- **c** Current children of the given directory will inherit its hoard status.
- **c+** Current and future children of the given directory will inherit its
  hoard status.
- **d** Current descendents of the given directory will inherit its hoard status.
- **d+** Current and future descendents of the given directory will inherit its
  hoard status.

For example, to hoard all of the `emacs` directory, its descendents and any
future descendents, I would include the following line in a hoardfile:

    a /coda/i386_mach/omega/usr/local/emacs 600:d+

This ensures you get all of the files you need, but you will use tens of
megabytes of cache space to hoard many files that you do not need, so often you
want to be more specific with respect to which files to hoard.

Other valid command to **hoard** are **clear**, **delete**, **list**, and
**modify**. See the [hoard.1](../manpages/hoard.1.md) man page for more details
on these commands.

## Hoarding for a Weekend

One of the most common uses of a Coda laptop is to take it home overnight or
for the weekend.  Naturally, you want to be sure that all of the files that you
need  over the weekend are in the cache; otherwise, there is little point in
bringing the laptop home. The **hoard** program helps you do this.  Create a
hoard file, as described in [Constructing a hoardfile](#constructing-a-hoard-file),
for each project you want to work on.

You may also want hoard files for your personal files, such as your home
directory if its in Coda, and other tools that you plan on using.  It is best
to clear the hoard database whenever you switch projects to make sure you have
enough space in your cache.  You might consider having **clear** as the first
command in your personal hoard file.  If you do, make sure you always run hoard
with this file before any other files.  Once you've built hoard files for all
of your tools and projects, its simply a matter of running hoard to build the
hoard database you want.  When you run **hoard**, you must be logged onto the
machines console, (dont run X).  About fifteen minutes before you are ready to
leave, force a hoard walk with the following command: `hoard walk`.

This will cause **venus** to attempt to cache all of the files in the hoard
database.  _Wait until the hoard command completes_ by looking at the Coda
client activity with **codacon**. You are now ready to disconnect from the
network.  You are encouraged to try all of the commands you intend on using
after you disconnect.  If you are missing some files, it will be easy to
reconnect and hoard them.

## Reintegrating After Disconnection

When you reconnect to the network after a disconnected session, Coda will
automatically try to reintegrate your changes with the Coda servers.  You must
be authenticated before reintegration occurs. Watch the file
`/var/log/coda/venus.err` or use the **codacon** command.  The reintegration
status should be written to either.

If the reintegration was successful, the log entries would look like:

!!! example

    ```
    Reintegrate u.raiff, (1, 244) ( 13:33:43 )
    coda: Committing CML for u.raiff ( 13:33:43 )
    coda: Reintegrate: u.raiff, result = SUCCESS, elapsed = 2640.0 (15.0, 2609.0, 15.0) ( 13:33:43 )
    coda:   delta_vm = 0, old stats = [0, 1, 0, 0, 0] ( 13:33:43 )
    coda:   new stats = [   0,   0.0,     0.0,    1,   0.2], [   0,   0.0, 0.0,  0,   0.0] ( 13:33:43 )
    ```

The following example is from a failed reintegration on the volume `u.raiff`.

!!! example

    ```
    Reintegrate u.raiff, (1, 244) ( 13:27:10 )
    coda: Checkpointing u.raiff ( 13:27:10 )
    coda: to /usr/coda/spool/2534/u.raiff@%coda%usr%raiff ( 13:27:10 )
    coda: Aborting CML for u.raiff ( 13:27:10 )
    coda: Reintegrate: u.raiff, result = 198, elapsed = 2437.0 (15.0, 2265.0, 531.0)  ( 13:27:10 )
    coda:   delta_vm = 1000, old stats = [0, 0, 1, 0, 0] ( 13:27:10 )
    coda:   new stats = [   0,   0.0,     0.0,    1,   0.2], [   0,   0.0, 0.0,   0,   0.0] ( 13:27:10 )
    ```

Notice that the _client modify log (CML)_ was checkpointed to
`/usr/coda/spool/2534/u.raiff@%coda%usr%raiff`.  This file is a tar file
containing the changes that were made on during the disconnected session.  The
files in the tar file are relative to the root of `u.raiff`.

The file which failed to reintegrate will be _in conflict_ and will need to be
repaired with the **repair** tool as discussed in [Repairing an Inconsistent
Directory](#repairing-an-inconsistent-directory).

## Dealing With a Flaky Network

When the network is acting up, you can use Coda to help isolate yourself from
the networking problems.  Set up your hoard database so that **venus** will
hoard the files you are working on.  Then, disconnect from the Coda servers
with the `cfs disconnect` command. To Coda, this is equivalent to physically
disconnecting from the network.

Once the network becomes stable, you can use `cfs reconnect` followed by `cfs
checkservers` to reconnect yourself to the Coda servers and reintegrate your
work. Do not forget to clear your hoard database with `hoard clear` once you
are done working on the set of files that you hoarded.

## Reintegrating Over the Phone Line

If you are planning on taking a Coda laptop on an extended trip, you should
consider using a dial-up connection like SLIP or PPP to reintegrate with the
Coda servers periodically. This will make your updates visible to other Coda
users, protect against client crashes such as hard drive failure or theft, and
allow you to work on multiple projects, even when your cache space is not large
enough for all of the projects.  By using the following instructions, you can
reintegrate over the phone and change the files in your hoard database.

 1. Dial in to your network remote access server.
 1. After network connection is established, tell Coda to see which servers it
    can communicate with `cfs checkservers`. Your laptop will now behave as if
    it is on the network.  Take into consideration that response time to
    commands will be much slower than on the LAN.

!!! important

    You can trigger immediate reintegration of specified directory by issuing
    the `cfs forcereintegrate <dir>` command.

## Repairing an Inconsistent Directory

Occasionally, a directory entry will become inconsistent.  This happens when
there is a conflict between file server replicas that Coda cannot automatically
resolve or a reintegration failed because of a local update the conflicts with
the global state.  The most common causes of a conflict are when  the file
servers are partitioned and a file is changed on more than one of the
partitions or when a disconnected client updates a file that is also updated on
the servers.  When this happens, the directory containing the conflict will now
look like a symbolic link and will be pointing to its _file identifier (FID)_.
For example if a directory, `conflict`, is inconsistent it will now appear
as:

``` sh
$ ls -l conflict
lr--r--r--  1 root      27 Mar 23 14:52 conflict -> @7f0000b3.00000005.0000011a
```

Most applications will return the error `File not found` when they try to open
a file that is inconsistent.  You need to resolve this conflict by using the
[repair.1](../manpages/repair.1.md) tool.

### Server/Server Conflicts

Once you run repair, you need to do a **beginrepair** on the object that is
inconsistent.  After **beginrepair** is issued, the inconsistent directory will
be _expanded_ to look like a directory with an entry for each of the volume
replicas.  You can look at all of these to decide which copy you want.  Use
**repair** choose the correct version and clear the inconsistency.  In the
following example the file `conflict/example` is replicated on two servers
and it has gone inconsistent.

!!! example

    ``` sh
    $ ls -l conflict
    lr--r--r--  1 root           27 Dec 20 13:12 conflict -> @7f0002ec.000000e3.000005d1
    $ repair
    The repair tool can be used to manually repair files and directories
    that have diverging replicas.  You will first need to do a "beginRepair"
    which will expose the replicas of the inconsistent object as its children.

    If you are repairing a directory, you will probably use the "compareDir" and "doRepair" commands.

    For inconsistent files you will only need to use the "doRepair" command.

    If you want to REMOVE an inconsistent object, use the "removeInc" command.

    Help on individual commands can also be obtained using the "help" facility.
    * begin conflict
    a server-server-conflict repair session started
    use the following commands to repair the conflict:
        comparedirs
        removeinc
        dorepair
    * ^Z
    Stopped
    $ ls conflict
    gershwin.coda.cs.cmu.edu schumann.coda.cs.cmu.edu
    $ ls conflict/*
    conflict/gershwin.coda.cs.cmu.edu:
    example

    conflict/schumann.coda.cs.cmu.edu:
    example
    $ fg
    repair
    compare
    Pathname of Object in conflict? [conflict]
    Pathname of repair file produced? [] /tmp/fix

    NAME/NAME CONFLICT EXISTS FOR example

    -rw-r--r--  1 raiff           0 Dec 20 13:10 gershwin.coda.cs.cmu.edu/example
    -rw-r--r--  1 -101            0 Dec 20 13:11 schumann.coda.cs.cmu.edu/example

    /coda/project/coda/demo/basic/rep/conflict/gershwin.coda.cs.cmu.edu/example
        Fid: (0xb0.612) VV:(0 2 0 0 0 0 0 0)(0x8002f23e.30c6e9aa)
    /coda/project/coda/demo/basic/rep/conflict/schumann.coda.cs.cmu.edu/example
        Fid: (0x9e.5ea) VV:(2 0 0 0 0 0 0 0)(0x8002ce17.30d56fb9)
    Should /coda/project/coda/demo/basic/rep/conflict/gershwin.coda.cs.cmu.edu/example be removed? [no] yes
    Should /coda/project/coda/demo/basic/rep/conflict/schumann.coda.cs.cmu.edu/example be removed? [no] no
    Do you want to repair the name/name conflicts  [yes]
    Operations to resolve conflicts are in /tmp/fix
    * do
    Pathname of object in conflict?  [conflict]
    Pathname of fix file? [/tmp/fix]
    OK to repair "conflict" by fixfile "/tmp/fix"? [no] yes
    SCHUMANN.CODA.CS.CMU.EDU  succeeded
    GERSHWIN.CODA.CS.CMU.EDU  succeeded
    * quit
    $ ls conflict
    example
    $
    ```

### Local/Global Conflicts

Local/global conflicts are caused by reintegration failures, which means that
the mutations performed while the client was disconnected are in conflict with
the mutations performed on the servers from other clients during the
disconnection. The objects involved in local/global conflict are represented in
the same fashion as server/server conflicts, i.e., they become dangling
symbolic links.

To start a local/global repair session for an object `OBJ`, you need to invoke
the **repair** tool and issue the **beginrepair** command with the pathname of
`OBJ` as the argument. Once the repair session is started, both the local and
global replicas of `OBJ` are visible at `OBJ/local` (read-only) and
`OBJ/global` (mutable and serving as the workspace for storing the repair
result for OBJ and its descendants).  The central process of repairing the
local/global conflicts on OBJ is to iterate the local-mutations-list containing
all the local updates performed on OBJ or its descendants, which can be
displayed by the `cfs listlocal` command. Each operation in the list must be
accounted for and the repair tool cooperates with **venus** to maintain the
current-mutation being iterated. The **checklocal** command can be used to show
the conflict information between the current-mutation and the global server
state. You can advance the iteration to the next operation using either the
**preservelocal** or the **discardlocal** command with the former replaying the
current-mutation operation on the relevant global replicas. You can also use
the **preservealllocal** and **discardalllocal** commands to speed up the
iteration. Because the global replica OBJ is mutable, existing tools such as
**emacs**, etc. can be directly used to make the necessary updates. The
**quit** command is used to either commit or abort the repair session. The man
page on on the repair commands contains more detailed information, and the
following simple example illustrates the main process of repairing a
local/global conflict.

!!! example

    Suppose that during disconnection, a user creates a new directory
    `/coda/usr/luqi/papers/cscw/figs` and stores a new version for file
    `/coda/usr/luqi/papers/cscw/paper.tex`. However, during the disconnection his
    co-author also creates a directory `/coda/usr/luqi/papers/cscw/figs` and stores
    some PS files in it. Upon reintegration a local/global conflict is detected at
    `/coda/usr/luqi/papers/cscw`.

    ``` sh
    $ ls -l /coda/usr/luqi/papers/cscw
    lr--r--r--  1 root           27 Dec 20 00:36 cscw -> @@7f000279.00000df3.0001f027
    $ repair
    * begin
    Pathname of object in conflict?  [] /coda/usr/luqi/papers/cscw
    a local-global-conflict repair session started
    the conflict is caused by a reintegration failure
    use the following commands to repair the conflict:
            checklocal
            listlocal
            preservelocal
            preservealllocal
            discardlocal
            discardalllocal
            setglobalview
            setmixedview
            setlocalview
    a list of local mutations is available in the .cml file in the coda spool directory

    * ls -l /coda/usr/luqi/papers/cscw
    total 4
    drwxr-xr-x  3 luqi         2048 Dec 20 00:51 global
    drwxr-xr-x  3 luqi         2048 Dec 20 00:51 local

    * listlocal
    local mutations are:

    Mkdir   /coda/usr/luqi/papers/cscw/local/figs
    Store   /coda/usr/luqi/papers/cscw/local/paper.tex (length = 19603)

    * checklocal
    local mutation: mkdir /coda/usr/luqi/papers/cscw/local/figs
    conflict: target /coda/usr/luqi/papers/cscw/global/figs exist on servers

    * discardlocal
    discard local mutation mkdir /coda/usr/luqi/papers/cscw/local/figs

    * checklocal
    local mutation: store /coda/usr/luqi/papers/cscw/local/paper.tex
    no conflict

    * preservelocal
    store /coda/usr/luqi/papers/cscw/global/paper.tex succeeded

    * checklocal
    all local mutations processed

    * quit
    commit the local/global repair session?  [yes]
    ```
