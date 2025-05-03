# Getting Started

## What is Coda?

Coda is a distributed file system, i.e. it makes files available to a
collection of client computers as part of their directory tree, but ultimately
maintains the authoritative copy of the file data on servers. Coda has some
features that make it stand out: it supports _disconnected operation_, i.e.
full access to a cached section of the file space during voluntary or
involuntary network or server outages. Coda will automatically reintegrate the
changes made on disconnected clients when reconnecting.  Furthermore, Coda has
read-write, failover server replication, meaning that data is stored and
fetched from any of a group of servers and Coda will continue to operate when
only a subset of all servers is available.  If server differences arise due to
_network partitions_, Coda will resolve differences automatically to a maximum
extent possible and aid users in repairing what can't be done automatically.
Coda is very differently organized from NFS and Windows/Samba shares.  Coda
does have many similarities to AFS and DCE/DFS.

## Getting clued in with the Coda terminology

**A single name space**.
All of Coda appears under a single directory `/coda` on the client (or under a
single drive under Windows). Coda does not have different exports or shares as
do NFS and Samba that are individually mounted.  Under `/coda` the volumes (aka
file sets) of files exported by all the servers (living in your Coda realm) are
visible (See Figure 1).  Coda automatically finds servers and all a client needs
to know is the name of one bootstrap server that gives it information how to
find the root volume of a Coda realm.

_Figure 1: Coda file-system view at a workstation._

``` mermaid
graph TD
  root((/));
  root --- usr;
  root --- bin;
  root --- etc;
  root --- tmp;
  root --- lib;
  root --- afs;
  root --- nfs;
  root --- misc[&hellip;];
  root --- coda;
  local([Local Files]);
  usr --- local;
  bin --- local;
  etc --- local;
  tmp --- local;
  lib --- local;
  afs --- shared1([Shared Files]);
  nfs --- shared2([Shared Files]);
  coda --- shared4([Shared Files]);
```

**Coda realm**.
A Coda realm is a group of servers sharing one set of configuration databases
(a single administrative domain).  A realm can consist of a single server or up
to hundreds of servers. One server is designated as the SCM - the System
Control Machine.  It is distinguished by being the only server modifying the
configuration databases shared by all servers, and propagating such changes to
other servers.

**Coda volumes**.
File servers group the files in volumes.  A volume is typically much smaller
than a partition and much larger than a directory.   Volumes have a root and
contain a directory tree with files. Each volume is "Coda mounted" somewhere
under `/coda` and forms a subtree of the `/coda` namespace.  Volumes can
contain mountpoints of other volumes.  A volume mountpoint is not a Unix
mountpoint or Windows drive - there is only one drive or Unix mountpoint for
Coda.  A Coda mountpoint contains enough information for the client to find the
servers which store the files in the volume.  The group of servers serving a
volume is called the Volume Storage Group (VSG) of the volume.

**Volume mountpoints**.
One volume is special, it is the root volume for a Coda realm, the volume which
Coda mounts at `/coda/<realm domain name/`.  Other volumes are grafted into the
`/coda` tree using `cfs mkmount <directory> <volume>`.  This command installs a
volume mountpoint in the Coda directory tree, and in effect its result is
similar to `mkdir mountpoint; mount device mountpoint` under Unix.  When
invoking `cfs mkmount` the two arguments given are the name of the mountpoint
and the name of the volume to be mounted.  Coda mountpoints are persistent
objects, unlike Unix mountpoints which need reinstating after a reboot.

**Data storage**.
The servers do not store and export volumes as directories in the local disk
filesystem, like NFS or Samba.  Coda needs much more meta data to support
server replication and disconnected operation and it uses complex recovery
mechanisms which are hard to do within a local disk filesystem.  Coda servers
store file data identified by a number in a directory tree in `/vicepa`.  The
_metadata_ (owners, access control lists, version vectors) and directory
content is stored in an memory-mapped RVM data file or partition.

**RVM**.
RVM stands for _Recoverable Virtual Memory_.  RVM is a transaction based
library to make part of a virtual address space of a process persistent on disk
and commits changes to this memory atomically to persistent storage. Coda uses
RVM to manage its metadata.  This data is stored in an RVM data file which is
mapped into memory upon startup. Modifications are made in memory and also
written to the RVM log file upon committing a transaction.  The RVM log file
contains committed data that has not yet been incorporated into the RVM data
file on disk.

**Client data**.
Data on the client is stored somewhat similarly: metadata in RVM (typically in
`/usr/coda/DATA`) and cached files are stored by number under
`/var/lib/coda/cache`.  The cache on a client is persistent.  This cache
contains copies of files from the server.  The cache allows for quicker access
to data for the client and allows for access to files when the client is not
connected to the server.

**Validation**.
When Coda detects that a server is reachable again it will _validate_ cached
data before using it to make sure the cached data is the latest version of the
file.  Coda compares cached version information (version vector) associated
with each object, with versions held by the servers.

**Authentication**.
Coda manages authentication and authorization through a token. Similar (but the
details are very different) to using a Windows share, Coda requires users to
log in.  During the log in process, the client acquires a session key, or
token, in exchange for a correct password. The token is associated with a user
identity, at present this Coda identity is the uid of the user performing the
log in.

**Protection**.
To grant permissions the cache manager and servers use the token with its
associated identity and match this against privileges granted to this identity
through _access control lists (ACL)_.  If a token is not present, anonymous
access is assumed, for which permissions are again granted through the access
control lists using the `System:AnyUser` identity.

## Organization of the client

### The kernel module and the cache manager

Like every filesystem, a computer enabled to use the Coda filesystem needs
kernel support to access Coda files.  Coda's kernel support is minimal and
works in conjunction with a userspace cache manager **venus**.  User requests
enter the kernel, which will either reply directly or ask the cache manager
**venus** to assist in service.

Typically the kernel code is in a kernel module, which is either loaded at boot
time or dynamically loaded when **venus** is started.  **venus** will also
mount the Coda filesystem on `/coda`.

### Utilities

To manipulate ACLs, the cache, volume mountpoints and possibly the network
behaviour of a Coda client a variety of small utilities is provided. The most
important one is the **[cfs](../manpages/cfs.1.md)** command.

There is also a **[clog](../manpages/clog.1.md)** command to authenticate to the
Coda authentication server.  The **codacon** command allows one to monitor the
operation of the cache manager, and **[cmon](../manpages/cmon.1.md)** gives
summary information about a list of servers.

## Server organization

The main program is the Coda fileserver **codasrv**.  It is responsible for
doing all file operations, as well as volume location service.

The Coda authentication server **auth2** handles requests from **clog** for
tokens, and changes of password from **cpasswd** and
**[au](../manpages/au.1.md)**.  Only the the **auth2** process on the SCM will
modify the password database.

All servers in a Coda realm share the configuration databases in `/vice/db` and
retrieve them from the SCM when changes have occurred.  The **updateclnt**
program is responsible for retrieving such changes, and it polls the
**updatesrv** process on the SCM to see if anything has changed.  Sometimes the
SCM needs a (non-shared) database from another server to update a shared
database.  It fetches this from an **updatesrv** process on that server using
**updatefetch**.

On the server there are utilities for volume creation and management. These
utilities consist of several shell scripts and the
**[volutil](../manpages/volutil.8.md)** command. There is also
**[pdbtool](../manpages/pdbtool.8.md)** to manipulate the user and group
databases.

## Authentication

Once you are logged in to your workstation, you need to get a Coda
authentication token by running **clog**.  **clog** will prompt you for
your Coda password and use it to get a token from the _authentication server_.
This token will expire in about 25 hours.  After the token expires, you must
use **clog** to obtain a new token for another 25 hours.

The following in an example of running **clog** twice.  The first time, the wrong
password was entered:

``` sh
$ clog
Password:
Invalid login (RPC2_NOTAUTHENTICATED (F))
$ clog
Password:
$
```

To see your newly acquired token, use **ctokens**.  This will display the
tokens and their expiration time.

``` sh
$ ctokens
Token held by the Cache Manager:

Local uid: 9010
Coda user id: 9010
Expiration time: Thu Apr  6 18:51:35 2000
```

Use the **cpasswd** command to change your Coda password. As with **passwd**,
**cpasswd** will prompt for your current password, then ask you to enter a new
password twice.

``` sh
$ cpasswd
Changing password for raiff
Old password:
New password for raiff:
Retype new password:
Password changed, it will be in effect in about 1 hour
```

You can "log out" of Coda by using the **cunlog** command to tell _venus_ to
forget your tokens.  Once you run **cunlog**, you will have the same privileges
as an anonymous Coda user until you acquire a new authentication token.

## Coda File Protection

Coda provides a close approximation to UNIX protection semantics. An _access
control list (ACL)_ controls access to directories by granting and restricting
the rights of users or groups of users.  An entry in an access list maps a
member of the protection domain into a set of rights.  User rights are
determined by the rights of all of the groups that he or she is either a direct
or indirect member of.  In addition to the Coda access lists, the three owner
bits of the file mode are used to indicate readability, writability, and
executability.  You can use **chmod** to set these permissions on individual
files.  Coda rights are given as a combination of `rlidwka` where:

- **r** - _Read_ allows the user to read any file in the directory.
- **l** - _Lookup_ allows the user to obtain status information about
  the files in the directory.  An example is to list the directory contents.
- **i** - _Insert_ allows the user to create new files or subdirectories in the directory.
- **d** - _Delete_ allows the user to remove files or subdirectories.
- **w** - _Write_ allows the user to overwrite existing files in the directory.
- **k** - _Lock_. The lock right is obsolete and only maintained for historical reasons.
- **a** - _Administer_ allows the user to change the directory's access control list.

Coda also has _negative_ rights, which deny access.  Any of the normal rights
listed above can also be negative.

Access control lists are managed with the **cfs** command with the **listacl**
and **setacl** options.  They can be abbreviated as **la** and **sa**
respectively.  To see the access control list of any directory in a Coda file
system, use `cfs la`.  The following example displays the current directory's
ACL:

``` sh
$ cfs la .
  System:AnyUser  rl
           raiff  rlidwka
```

The displayed list, shows that the user "raiff" has all of the access rights
possible on the directory and that the group `System:AnyUser` has read and
lookup privileges.  `System:AnyUser` is a special Coda group, that includes all
users.

A second example shows another group, `System:coda`. Anyone who is a member of
the group, will have the groups access rights:

``` sh
$ cfs la /coda
     System:coda  rlidwka
  System:AnyUser  rl
```

Use `cfs sa` to change or set a directory's access control list.  Options to
`cfs sa` include **-negative** to assign negative rights to a user and
**-clear** to clear the access list completely before setting any new access
rights.  You can also use **all** or **none** to specify all rights or no
rights respectively.

To remove `System:AnyUser` access to the current directory, you would issue the
following command:

``` sh
cfs sa . System:AnyUser none
```

To give System:AnyUser read and lookup rights, use:

``` sh
cfs sa . System:AnyUser rl
```

To deny rights to a user, use the **-negative** switch:

``` sh
cfs sa -negative . baduser rl
```

This will deny `baduser` read and lookup rights, even though any other user has
these rights.  Note that negative rights are maintained separately from the
normal rights, so to re-instate badusers' read and lookup access,
you must use:

``` sh
cfs sa -negative . baduser none
```

If you omit the **-negative** switch, then `baduser` will still be denied read
and lookup access.

## Disconnected Operation

If all of the servers that an object resides on become inaccessible, then the
client will use the cached copy of the object (if present) as a valid replica.
When the client does this, it is operating in _disconnected mode_.

Disconnected mode may be the result of a network failure or it could be the
result of intentionally removing a laptop from the network.  If you make sure
all of the files you want to use are cached on your laptop, you can travel with
it and access your files as if you were still on the network.

Unfortunately, a cache miss while operating in disconnected mode is not
maskable, and you will get a `Connection timed out` error message.  Coda allows
you to mark (_hoard_) files with caching priorities to help keep the ones you
want in the cache.

When you are in disconnected mode, you may want to checkpoint the _modify log
(CML)_ that Coda keeps to track which files and directories have changed while
disconnected.  Use `cfs checkpointml` to do this.

Checkpointing the modify log will ensure that changes you have made will not be
lost if the cache manager crashes severely.  Coda uses this modify log when it
re-integrates with the servers.

Coda adapts easily to low bandwidth connections like (PPP or SLIP modem links).
You can use this to periodically reintegrate changes or cache new files when
you are on a trip.

When you reintegrate after operating in disconnected mode, run **codacon** to
see the progress of your reintegration.

If reintegration was not successfull, the files that you modified will be put
in a tar file in `/var/lib/coda/spool/<uid>`.  Reintegration fails, for example,
when you modified a file in disconnected mode while someone else also modified
that file on the servers.  Read [Reintegrating After
Disconnection](scenarios.md#reintegrating-after-disconnection) for more
information on reintegration.

## Hoarding

Coda allows you to advise the cache manager, **venus**, of critical files that
it should  try to keep in the cache.  You indicate the relative importance of
the files by assigning priorities to them.  This is known as _hoarding_.
**venus** maintains an internal _hoard database_ of these files.  Hoarding a
file helps to ensure that it will be available when operating in disconnected
mode.  See the [hoard.1](../manpages/hoard.1.md) manual page and the
[Constructing a hoard file](scenarios.md#constructing-a-hoard-file) and
[Hoarding for a Weekend](scenarios.md#hoarding-for-a-weekend) sections in this
document for an example of how to set up your hoard database.  A convenient way
of setting up your hoard database is by creating a file with commands for
**hoard**.  This file is known as a _hoard file_.

## Repairing Conflicts

As a result of Coda's optimistic replica management, object replicas can
conflict between servers.  A conflict arises when the same object is updated in
different partitions of a network.  For instance, suppose a file is replicated
in two locations (say, serverA and serverB).  If these two sites become
partitioned and a user on each side of the partition updates the file (userA
updates the file on serverA while userB updates the file on serverB), the file
will be in conflict when the partition ends.  Conflicts may also arise at the
end of disconnected operation.

Coda guarantees conflict detection at the first request for an object when both
servers are accessible.  When a conflict is detected, Coda attempts to perform
automatic conflict resolution.  In simple cases, the conflict will be resolved
automatically, a process which is transparent to the user except for a time
delay in accessing the object.  However, in more difficult cases, automatic
conflict resolution fails and the object is marked _in conflict_.  File system
calls on an object which is in conflict fail with the same error code as if the
object were a dangling, read-only symbolic link (usually, `File not found
(ENOENT)`).  The conflict must be resolved by a user who has appropriate access
to the object.  To help users resolve conflicts, Coda provides a repair tool
which is discussed in [Repairing an Inconsistent
Directory](scenarios.md#repairing-an-inconsistent-directory).
