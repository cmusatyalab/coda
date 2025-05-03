---
title: Server Installation
---
# Installing a Coda Server

!!! danger
    This page contains outdated info and needs to be updated and rewritten.

## Introduction

A Coda realm is a set of servers which all believe one member of the set
to be the master, or SCM server.  All modifications to important Coda databases
should be made on the SCM, otherwise the SCM plays the role of an ordinary
Coda server.  The **updateclnt** and **updatesrv** daemons will then propagate
changes from the SCM to the other servers.

The first server setup must be the SCM.  This chapter is divided into three
sections: installing the binaries, configuring the system using the
configuration script **vice-setup** (for both SCM and non-SCM servers) and
finally, a detailed look at what needs to be done to setup a server manually.
Before beginning, we have provided some overview information to assist in
setting up the server.

### Recoverable Virtual Memory

To help ensure that data is not lost or left in inconsistent state between
server restarts, Coda uses _Recoverable Virtual Memory (RVM)_ which is a
transaction system that maintains the system state of the Coda server meta
data.  RVM is a data logging transaction system which writes modifications into
the RVM log and upon truncation or restart incorporates such modifications into
the RVM data file.  Both log and data file for RVM are ideally held on raw
partitions.

!!! note

    This should not be confused with Virtual Memory.

Upon startup, the Coda servers use RVM to restore the Coda system state.  For
optimal performance you should have dedicated disk partitions for metadata and
log partitions, ideally log partition should reside on its own disk.  However,
a disk sharing the log partition with other disk partitions or the use of a log
file may be used at corresponding loss of optimal performance.

**RVM metadata storage.**
This is a file or raw partition for RVM metadata.  You can use a file but it
will be quite slow on a larger server. This partition must be around 4% of the
total size of the files you wish to store under `/vicepa` (e.g. on a 2GB server
we use around 80MB of rvm data). For first installations we recommend the
default 22MB options, and using _files_ for RVM log and data.

!!! note

    Windows NT Setup creates the file `C:\coda\rvm\DATA`. Use this for RVM
    metadata.

**Virtual memory.**
The metadata, held in the RVM data file, is memory mapped.  You need that
amount of space as virtual memory on your system, in addition to virtual memory
to run the server (~6MB) and other software.

**RVM transaction log.**
This is a LOG file, preferably a raw partition on a disk by itself. This needs
not be large, a few MB's are fine.

!!! note

    Windows NT Setup creates the file `C:\coda\rvm\LOG`. Use this for RVM
    transaction log.

Please refer to [RVM Initialization](#rvm-initialization) for more details on
RVM.

### Server Disk Organization

Coda servers require a minimum of 2 disk partitions for optimal performance
(one raw partition for the RVM Log, one raw partition for RVM Data and one
regular UNIX file system to hold file data store in Coda), data security and
protection from accidental deletion.  For additional performance gains the RVM
Log partition should be on its own disk to avoid head movement latencies which
reduces disk seek times on log operations.  Optionally, `/vice` can be a
separate partition for the same reasons it is advantages to have `/var` as a
separate partition.

However, other configuration may be used such as having the RVM Data and Log
information stored as regular UNIX files at a loss in performance and data
security.  Also, if more than one Storage Area Data is needed on a Coda Server
(the default directory is called `/vicepa`), the additional storage areas must
be separate partitions (different partition from `/vicepa` the default, initial
storage area for data under Coda) and mounted as `/vicepb`, for example.

The table below shows a possible partitioning of disks on Coda servers with
their respective purpose, mount points, typical sizes and consistency check
program.

!!! note

    Sizes of these partitions were taken from one of the Coda servers at
    CMU-SCS, the actual sizes may vary at other installations.

_Table 2: Example of Partitions for a Coda Server_

| Partition | Storage Purpose | Mounted | Typical Size | Is checked |
| --------- | --------------- | ------- | ------------ | ---------- |
| hda2      | Root and User File System | / | 650MB    | Yes        |
| hda5      | Var File System | /var    | 100MB        | Yes        |
| hda3      | Vice File System | /vice  | 300MB        | Yes        |
| hdc1      | RVM Log         | -       | 12MB         | No         |
| sda1      | RVM Data        | -       | 130MB        | No         |
| sda2      | Coda FS Data 0  | /vicepa | 1.6GB        | Yes        |
| sda3      | Coda FS Data 1  | /vicepb | 1.6GB        | Yes        |
| sda5      | Coda FS Data 2  | /vicepc | 1.6GB        | Yes        |

## Installation and Configuration

!!! tip

    Always check the INSTALL and README files on
    (<ftp://ftp.coda.cs.cmu.edu/pub/coda>&lt;platform&gt;) for last minute
    changes and updates that have not yet found their way into the manual.

### Installing the Coda Server Binaries and Documentation

Currently, server files for the supported platforms are installed into the
following directories:

=== "Linux"

    - `/usr/bin` - user applications
    - `/usr/sbin` - system binaries
    - `/etc/coda` - configuration files

=== "FreeBSD"

    - `/usr/local/bin` - user applications
    - `/usr/local/sbin` - system binaries
    - `/usr/local/etc/coda` - configuration files

=== "NetBSD"

    - `/usr/local/bin` - user applications
    - `/usr/local/sbin` - system binaries
    - `/usr/local/etc/coda` - configuration files

!!! note

    Please make sure your PATH environment variable is set properly for your
    platform.  That is, ensure FreeBSD has `/usr/local/{bin,sbin}` and NetBSD
    has `/usr/pkg/{bin,sbin}` in its default search paths.  Linux installs Coda
    into the default system paths, so Linux should not need its default search
    path modified.

There is one directory used by all platforms: `/vice` which is used to store
the supporting Coda configuration and Coda databases.  It does not need to be
in the PATH.

Here are the specific steps to install the binaries and documentation on
supported platforms:

=== "Linux"

    ``` sh
    # rpm -Uvh lwp-<release>.i386.rpm
    # rpm -Uvh rvm-<release>.i386.rpm
    # rpm -Uvh rpc2-<release>.i386.rpm
    # rpm -Uvh coda-debug-server-<release>.i386.rpm
    ```

=== "FreeBSD"

    Build and install `coda5_server` port, other ports will be built and
    installed as dependencies:

    ``` sh
    # cd /usr/ports/net/coda5_server
    # make
    # make install
    ```

=== "NetBSD"

    Build and install `coda5_server` source package, others will be built and
    installed as dependencies:

    ``` sh
    # cd /usr/pkgsrc/net/coda5_server
    # make
    # make install
    ```

### Configuring a Coda Server

Server setup is similar for all platforms.

#### Before Beginning

The `/vice` directory will be created by the setup script automatically if it
does not already exist.  However, if your root partition does not have enough
additional space (e.g.  the `SrvLog` can become huge - over 10MB under certain
circumstances), you will need to create a partition to be mount as `/vice`,
format the partition as normal UNIX file system, create the mount point
`/vice`, add the partition and mount information to `/etc/fstab` and mount it
by hand _BEFORE_ running the setup script.

Alternatively, `/vice` may be a symbolic link to a directory on an existing
partition with enough space but it also must be created before running the
setup script, **vice-setup**.  Sub-directories needed under `/vice` will be
created by **vice-setup**.

Also note that the setup of an SCM differs in some important ways from a
non-SCM setup.  So, when **vice-setup** asks: "Will this machine will be the
SCM?" it is very important this question is answered correctly.

While **vice-setup** sets up things that are common for both the SCM and
non-SCM servers,  different helper scripts will be called depending on whenever
the answer to question "Is this the SCM?" is _yes_ or _no_.

=== "**vice-setup** SCM"

    ```
    vice-setup-scm
    vice-setup-user
    vice-setup-rvm
    vice-setup-srvdir
    ```

=== "**vice-setup** non-SCM"

    ```
    vice-setup-rvm
    vice-setup-srvdir
    ```

**vice-setup** invokes these scripts in the order listed above.

#### Setting Up a Coda Server

If this is the first (or only) server being setup for a Coda realm, it must be
setup as the SCM by answering _yes_ to the question when asked by
**vice-setup**.  The SCM coordinates the sync'ing of Coda database and global
configuration files needed to keep  track of data stored in Coda.  The SCM also
coordinates the authentication of a Coda user.

If you are adding a non-SCM server which keeps copies of databases, but does
not distribute them to the rest of the machines comprising a Coda realm answer
_no_ to the question.

Other than the distinctions indicated above, the SCM plays no special role in a
Coda realm and can actually be down for a short time without denying service in
a multiple-machine realm site.  However, is extremely important to only have one
SCM.

!!! note

    Running a server seriously dips into system virtual memory.  Running a Coda
    server, a Coda client *and* X11, we have observed  that a slightly over
    64MB of available VM is needed.  The **top** command gives information on
    memory, CPU and process usage.  Therefore, we do not recommend running a
    Coda client on a Coda server, and only run X when performing server
    maintenance, if X is convenient to use.

Whether setting up an SCM Coda server or adding a server to an existing Coda
realm, make sure the following items are taken care before running the setup
script:

1. Create an empty directory (e.g. `/vicepa`) where the file server will keep
   user files.  If more disk space is required than the size of single
   partition, additional directories on separate partitions can be created
   (e.g. `/vicepb`).

1. Allocate a raw partition for RVM meta-data (you can use a file but this will
   be slow on a medium to large server).  This partition must be around 4% of
   the total size of the files you wish to store under `/vicepa` (e.g.  on a
   3.3GB server we use around 130M of rvm data).  Consider 10M to be the minimum.

1. Allocate a LOG partition, preferably on a separate disk.  This needs not be
   large (12MB is large enough for a 3.3GB partition which is the largest
   default configuration provided by **vice-setup-rvm** script).

1. Prepare two different secret tokens of up to 8 characters long (e.g.
   _elephant_).  The RVM files involve the journaling/transactional aspects of
   Coda and communicating this information between servers and via the
   loop-back device when the SCM updates itself.  The tokens are used to secure
   server-server communications.

1. If the root partition is not large enough to hold `/vice`, having
   alternative measures setup as outlined in [Before Beginning](#before-beginning)

Once you have the above items available, run **vice-setup**>.  This command
provides several useful "canned" configuration for setting up a Coda server.
It does a number of things "behind the scenes" as well (such as setting up the
directory `/vice`, if it does not exist and populating it with the
sub-directories `backup db srv vol bin`) while asking the following questions:

    Enter a random password for auth2 authentication:
    This must be up to eight characters in length.

    Enter a random password for volutil authentication:
    This must be up eight characters in length and different than the auth2
    password.

    Enter a random password for update authentication:
    This must be up eight characters in length and different than the auth2 and
    volutil passwords.

    Do you want to start the server at boot time? (y/n)

Answering _yes_ to this question creates the file `/vice/srv/STARTFROMBOOT`,
which must be present for either **rc.vice** or **vice.init** to start the
server at boot time.  Removal of this file will prevent the Coda server from
starting automatically at boot time.

    Is this the master server, aka the SCM machine? (y/n)

For the first Coda machine being setup in a Coda realm, the answer must be _yes_.
If you are adding a server to an existing Coda realm, the answer must be _no_.

If you have answered _no_ to the above question, you will be asked for the
HOSTNAME of an existing SCM:

    Enter the hostname of the SCM machine:

This question is only asked if the machine is not the SCM.  The host name
entered must be that of the SCM which distributes the Coda realm database and
global configuration files.

If you have answered _yes_ to the question about setting up master SCM machine,
you will see the message: "Now installing files specific to the SCM..."
and get following questions:

    serverid:

All servers in a Coda realm need to have a unique number to identify them.  The
servername to identifier mappings have to be defined by the administrator in
the file `/vice/db/servers` on the SCM according to the following format:

    <servernameX>.domain.name    1
    <servernameY>.domain.name    2
    ...

There are currently several limitations to which identifiers are actually
usable:

- all numbers must fit in a single byte.
- 0 and -1 (255) are used in error conditions.
- 127 is used to identify `replicated volumes'.

This leaves us with a usable range of 1-126 and 128-245 for server identifiers.

    Enter the name of the rootvolume (< 32 chars):
    Enter the name of your root volume (e.g. coda.root)

After creating root volume system will create privileged user for managing Coda
servers. You will see following message:

    You need to give me a uid (not 0) and username (not root) for a Coda
    System:Administrator member on this server, (sort of a Coda super user)
    Enter the uid of this user:

We recommend you to enter uid of your local account, using which you will
administer Coda servers.

    Enter the username of this user:
    Enter your username

If operation was successful, you should see following message:

    An initial administrative user <username> (id <uid>) with Coda password
    "changeme" now exists.

    Are you ready to set up RVM? [yes/no]

RVM is the Recoverable Virtual Memory used to keep a copy of Coda Virtual
memory on local disk.  Answer _yes_ to this question or the setup will abort.

    What is your log partition?

While RVM can be placed in regular file, it is strongly recommended to allocate
a small partition on a dedicated disk for this purpose.  If you have set aside,
for example, `/dev/sdc1` under Linux as the log partition, enter the
partitions' device name at the prompt.

!!! important

    If you are setting up a \*BSD system, you must enter the raw partition
    name.  For example, if you are going to use `/dev/sd2e` you must enter the
    raw device name `/dev/rsd2e`.

The maximum template size we provide for and have tested is 30MB.

    What is your log size? (enter as e.g.  '12M')

The log partition keeps transaction records for Coda volume updates that may
not have been written to the Coda data partition yet.  We have not extensively
tested values above 30MB and do not recommend more than this.  12MB is a good
value for a small to medium sized server.  The value may be entered as,
_12M_ or _12288_ for 12 Megabytes.  Read [RVM Metadata
partition](#the-metadata-partition) for a more detailed explanation.

    What is your data partition (or file)?

This allows you to specify a partition or file.  We strongly recommend a
partition which can reside with other system partitions.  Remember, if you use
a partition, you are using it "raw".  it does not contain a file system of any
kind.  Again, if you are using Linux, you may enter a block device name such as
`/dev/sdc1`.

!!! important

    If you are using a *BSD system, you must enter a raw device such as
    `/dev/rsd2e`.

    What is the size of you data partition (or file) [22M,44M, 90M, 130M]:

This specifies the the size of the RVM data partition.  It must be typed
exactly as _22M_, _44M_, _90M_, or _130M_.  These are associated with default
parameters that must be feed to **codasrv** when it is started.  The following
table provides the log size to total data storage conversion:

- _22M_ is good for up to 500MB.
- _44M_ is good for up to 1.0GB.
- _90M_ is good for up to 2.2GB.
- _130M_ is good for up to 3.3GB.

For a detailed explanation of the trade off between different Data log sizes
vs.  Data Storage Space, please see [RVM Log partition](#the-log-partition).

    Proceed, and wipe out old data? [y/N]
    WARNING if you have entered erronous entries for RVM log and RVM data
    partitions, you could damage your system.  Be sure you entered correct
    information!
    This is the last chance before any action is taken by the setup script.
    Enter "y" to commit or "N" to abort.
    Where shall we store your data [/vicepa]?

Older development versions of coda require `/vicepa` to be present.  This
version does not.  You give a name other than the default we suggest.  However,
we strongly advise not calling it `/coda` as that can be confused with venus.

Once you successfully complete **vice-setup** you can start your Coda
file-server.

#### Running the Coda Server

##### Startup

Start the update server, update client and the auth server, as
well as the fileserver by typing:

=== "Linux"

    ``` sh
    # /etc/rc.d/init.d/auth2.init start
    # /etc/rc.d/init.d/update.init start
    # /etc/rc.d/init.d/codasrv.init start
    ```

=== "FreeBSD"

    ``` sh
    # /usr/local/etc/rc.vice start
    ```

=== "FreeBSD"

    ``` sh
    # /usr/pkg/etc/rc.d/rc.vice start
    ```

=== "Windows NT"

    ``` sh
    > codastart
    ```

!!! note

    Coda file-server process is started using following command:

    ``` sh
    codasrv -rvm <log_partition> <data_partition> <size_of_data_partition>
    ```

    To facilitate automatic setup procedure and easy changes of configuration
    we provide a script called **startserver**, which reads these command-line
    arguments from the file `/etc/coda/server.conf` and provides them for
    **codasrv**.

    The **vice-setup-rvm** creates `/etc/coda/server.conf` with values derived
    from information you have entered during setup process.  Should you deviate
    from one of these canned default values, you will need to either create or
    modify the `/etc/coda/server.conf` file to indicate the correct data size
    of the data partition.  For example, one of CMU's Coda servers contains:

    ```
    rvm_log=/dev/hdc1
    rvm_data=/dev/sda1
    rvm_data_length=119070700
    ```

    You should be careful to specify the correct files or partitions for the
    RVM log, data segment, and size of the data segment.

The `SrvLog` should show "File Server started".  If not, you have a problem.

!!! info "Log Files"

    Each process running on a server maintains a log file of its activities.
    This file may be monitored by running **tail -f** on the particular file.
    For instance, an operator might want to run this command in a small window
    on the file server console.  The table below shows the absolute pathname of
    the log file for each server process.

    | Server Process | Log File            |
    | -------------- | ------------------- |
    | File Server    | /vice/srv/SrvLog    |
    | Authentication | /vice/auth2/AuthLog |
    | Update Client  | /vice/misc/UpdateClntLog |
    | Update Server  | /vice/misc/UpdateSrvLog |


    The processes append new log entries to the end of the log file.  To
    control the growth of the log files over extended periods of operation, the
    files should be removed periodically. `SrvLog` may be removed by the
    **swaplog** option of the **volutil** command; this will move the current
    `SrvLog` to `SrvLog-1` and begin a new log.  Swaplog also keep the last 6
    log files, by removing, `SrvLog-7` and renaming the remaining log files. A
    swaplog is also performed during file-server startup.  The log files of the
    other processes should be removed only when their respective processes are
    not running.

For a non-SCM server this startup sequence will cause the new server to first
obtain the current server files such as `/vice/db/ROOTVOLUME` and
`/vice/db/VRList` as well as others needed for this server to participate in
the Coda realm.

If the **codasrv** process started correctly, you should see "File Server
started" in the SrvLog, othervise you will need to look at the error messages
in `SrvLog` and `SrvErr` to determine what went wrong.  If the messages are not
helpful, you should restart the server with higher debugging level.  Consult
[Troubleshooting](troubleshooting.md) about problem solving.

!!! note

    In case of trouble, it is worth first to check with **ps** whenever
    **auth2**, **updatesrv** and **updateclnt** are running.

!!! info "Changing debugging level of a Coda server"

    The **codasrv** allows you to specify the level of debugging information
    which is logged.  Level 0 is the lowest level of debugging available (i.e.
    no extra information is printed), while the highest level of debugging
    available is 1000 (i.e. all debugging information is printed).  As with all
    other server output, debugging output will appear in `SrvLog`.  Be careful
    to turn off debugging when its output is not in use since the log will grow
    rapidly and can easily fill the partition with debugging information.

    There are two ways to change debugging level of the file-server:

    1. Re-starting server with **-d &lt;debug_level&gt;** (e.g. following command
       **startserver -d 10 &** will provide reasonable amount of additional
       information)
    1. changing debugging level on the fly via **volutil setdebug &lt;debug_level&gt;**.

One final step is needed to make the SCM server function.  Use the root volume
you specified in **vice-setup** in place of &lt;your-root-volume&gt;.  This
step creates the volume, which will be mountable by the client software.

    # createvol_rep <your-root-volume> E0000100 /vicepa

Once you have done this, check if it was successfully created by viewing the log:

    # tail -f  /vice/srv/SrvLog &

##### Shutdown

To shutdown the file server gracefully, use the volume utility client
procedure, **volutil**.  Issue the command **volutil shutdown** and then
monitor the log file for the server process (`/vice/srv/SrvLog`) until it
reflects that the server has shutdown completely.  For example, here are the
last few lines before file server shutdown:

    VShutdown: Taking volume usr.grajen(0xcc0006d1) offline...... Done
    VShutdown: Taking volume synrgen.rep(0xcc00057c) offline...... Done
    VShutdown: Taking volume i386.omega(0xcd0004fc) offline...... Done
    VShutdown: Taking volume trace.rep.0(0xcc00057d) offline...... Done
    VShutdown: Taking volume usr.mmv.rep.0(0xcc00057e) offline...... Done
    VShutdown: Taking volume trace.backup(0xcc00057f) offline...... Done
    VShutdown: complete.

### Exploring Replication

First you will have to add a second server to your Coda cluster.  Install the
software and use **vice-setup** again.  This time your server is _not_ going to
be the SCM.  Proceed answering the questions until done.

#### Adding a Coda Server to a Realm

!!! note

    All actions should be performed on SCM, unless another server is mentioned.

1. Your server needs a unique server number, to be added to the
   `/vice/db/servers` file.
1. Make two new entries in the `/vice/db/VSGDB` file.  One for your new server
   by itself, one of the form: _E0000104 &lt;scm-server&gt;
   &lt;second-server&gt;_.  _E0000104_ is an example of unique id assigned to
   the _Storage Group_ containing our two servers. Each set of servers that
   contains replicated volumes should be assigned its unique id.
1. Start **updatesrv** and **updateclnt** on the second server.
1. Start **codasrv** on the second server.
1. Make a new volume using **createvol_rep** giving the address of the volume
   as _E0000104_.
1. Mount the volume.

You can now use this volume and your files will automatically be stored on
multiple servers.  To temporarily disable a server, and see that things
continue to function normally, either shut the server down with **volutil
shutdown** or disconnect its network.  You can also isolate the server using
our network filters with **filcon isolate -s server-name**.  Using **filcon
clear &lt;server-name&gt;** clears the filter.

Modifications made to Coda files during the server outage will be resolved when
the files are first accessed.  You see message of the form "Resolve path" in
the codacon output.  By typing **cfs checkservers** you can see if the server
is available again.

## Underneath vice-setup

The sections contained here describe what **vice-setup** does for you.  This
information is useful for those who either wish to customize our default
vice-setup script or wish to have a custom server setup outside the scope of
Coda's setup script.

### RVM Initialization

RVM initialization requires the selection of several parameters, each of which
involve tradeoffs.  Although the RVM log and data can be kept as regular files,
raw partitions have much stronger data consistency guarantees as well
performance advantages.  It is probably best to plan out the RVM partitions on
paper first, taking into consideration both the effect on RVM performance as
well as overall disk usage if you choose not to use the values provided in the
**vice-setup** script.

#### The Log Partition

The size of the log device is based on available space and issues involving
truncation.  Larger logs provide a longer accessible history of operations, are
truncated less frequently, but each truncation will take a longer period of
time.  Shorter logs truncate more often, but each truncation takes less time.
Log size is also strongly related to server startup time as well.  We use a 90M
log size, on a storage size roughly 3.2GB spread between two UNIX partitions.
(We suggest leaving a little space at the end of the RVM log partition for
safety, as RVM automatically adds about one extra page to the amount you
specify).  Our **vice-setup** script provides for several default values, the
largest of which is 130MB.  If you decide to follow the recommendation and use
a dedicated partition, creating a 130MB partition will leave plenty of room to
expand even if your initial use only suggests a size of 22M, for example.

The log is initialized with **rvmutl**.  At **rvmutl's** prompt, use the
command **i**, and then specify the size of the log segment.  In specifying the
size, you can use _M_ for megabyte and _K_ for kilobyte.  For example, to
initialize a log on partition `/dev/sdc1` to 22 megabytes:

``` sh
# rvmutl
* i
Enter name of log file or device: /dev/sdc1
Enter length of log data area: 22M
* q
```

#### The Metadata Partition

The data segment contains the meta-data of the system such as volume headers,
Coda directories, resolution logs,  etc.  The size of the data  segment depends
on the amount of disk space for file data, i.e.  the size of the `/vicep<?>`
partitions.  As a rule of thumb, you will need about 3-5% of the total file
data space for recoverable storage.  We currently use 4% as a good value under
most circumstances.  In our systems the data segment is 90Meg for approximately
3.2 gigabytes of disk space.  By making it smaller, you can reduce server
startup time.  However, if you run out of space on the RVM Data partition, you
will be forced to reinitialize the system, a costly penalty.  So, plan
accordingly.

In initializing (or reinitializing), you need to pick several parameters.  The
first is the starting address of the recoverable segment in your address space.
On our servers we start the RVM segment at _0x50000000_ for both Linux and BSD
based systems on Intel architecture.  The second is the amount of space to give
the recoverable heap.  The heap will obviously grow over use, so plan
accordingly.  Our heap space is _0x1000000_.  We suggest that for the last
parameters you use 1Meg (_0x100000_) for the static area, use _80_ free lists
(or nlists), and a chunksize of _32_.  These numbers will work well with the
internal structure of the file server and are provided as the default vales in
the setup script **vice-setup**.

To perform the data initialization, run the program **rdsinit**.  **rdsinit**
takes two parameters, the names of the RVM log and data devices.  For example,
to initialize one of our Intel based servers:

``` sh
# rdsinit /dev/hdc1 /dev/sdb1
Enter the length of the device /dev/hdc1: 119070700
Going to initialize data file to zero, could take awhile.
done.
rvm_initialize succeeded.
starting address of rvm: 0x50000000
heap len: 0x1000000
static len: 0x100000
nlists: 80
chunksize: 32
rds_zap_heap completed successfully.
rvm_terminate succeeded.
```

!!! note

    Use of the decimal value for the length of the device and the use of hex
    values for the address and lengths of the next three values.

### Update Monitor

The update monitor is used to propagate changes to the Coda server databases to
all servers from the SCM.  Client update processes run on all the Coda servers
and connect to a server update process running on the SCM.  The server process
uses the file `/vice/db/files` to determine which files should be kept
consistent on all the servers.  See the
[updateclnt.8](../manpages/updateclnt.8.md) and
[updatesrv.8](../manpages/updatesrv.8.md) man pages for more details.

Create the file `/vice/db/files` on the SCM.  Currently our `/vice/db/files`
looks like this:

    ROOTVOLUME
    VLDB
    auth2.pw
    auth2.tk
    auth2.tk.BAK
    auth2.lock
    servers
    prot_users.db
    prot_index.db
    volutil.tk
    update.tk
    VRDB
    files
    VSGDB
    dumplist

### Authentication Database

Coda uses an authentication database that is separate from the UNIX password
file.  This database is maintained by the SCM.  When someone authenticates to
Coda, their password is checked against this database and that person is issued
a token if they successfully authenticate.

On servers other than the SCM, the **Update** monitor will ensure that the
proper database files are propagated from the SCM.  However, you must copy the
file `/vice/db/update.tk` to the new server for the **Update** monitor to
work.
