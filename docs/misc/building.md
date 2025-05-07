# Building the Coda Filesystem

_A very brief overview._

## What gets build?

<!--
### kernel-src

A networked filesystem will contain a fs layer, which can be addressed
by the VFS layer (or the IFS manager in Windows 95) to get system
calls working.

Additionally Coda has operations on inodes, which are simple system
calls implemented at kernel level.  These we hope to fade out when we
have a btree based user level umulation of these calls. An
oversimplified example of this user ifs  (inode filesystem) library is
in the coda-src.

These system calls have two legs: the kernel implementation of the
calls and secondly the stub library to make new system calls.

Finally on Mach only, a pioctl system call was implemented.
-->

### lib-src

First a base library with common functions is build.

#### lwp

Next is the threads package.  There are two versions of this. Old is
purely co-routine, new is layered on cthreads.  New is not used by
Coda, but maintained for historical reasons.  A similar layering of
LWP threads on pthreads will be needed in the future, allowing both
forced coroutine behavior and preemption.

The thread package is used by RPC2, RVM, and Coda, and must be compiled
before either of these.

There are two little test programs for the threads, rw and test. The
latter takes two arguments, the first none.

#### rpc2

Contains the RPC2 remote procedure call library.

#### rvm

In RVM are the sources for the transaction package.  All Coda
platforms build the plain and LWP threaded versions of the
libraries. I believe only the LWP threaded version is used by Coda.

For HPUX there is a version of RVM using pthreads, which could be
useful to Coda in the future, doing log truncation in a kernel thread
could provide a major performance boost.

A key issue leading to confusion is the file cthreads.h which is
included by the rvm\_basher program.  Since the cthreads version of RVM
is not used anywhere, we have changed this to a dummy\_cthreads. file,
which is installed when the RVM libraries are built.

The most realistic test for RVM is the lwp\_basher with runs a threaded
RVM.

### coda-src

This builds the Coda file system.  Major sub packages are:

#### venus

The Coda client.

#### vice

The Coda server.

#### vicedep

Common RPC2 protocol definitions for both client and server.

#### auth2

Authentication daemon and authentication related user tools (**clog**,
**ctokens**, **cpasswd**).

#### update

Update daemons to synchronize configuration between Coda servers.

#### norton

Tools to directly manipulate RVM memory on the Coda server.

<--
al
asr
asrlauncher
dir
egasr
kerndep
librepair
lka
partition
repair
resolution
scripts
smon2
util
vcodacon
vol
volutil
vtools
vv
-->
