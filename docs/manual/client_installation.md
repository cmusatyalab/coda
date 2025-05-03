---
title: Client Installation
---
# Installing and Configuring a Coda Client

!!! danger
    This page contains outdated info and needs to be updated and rewritten.

A Coda client can be run on any workstation that has kernel support for Coda in
its kernel.  Supported UNIX^TM^-like platforms are currently Linux, FreeBSD and
NetBSD.

This chapter assumes the chapter on [Kernel Configuration](kernel.md) has been
read.  It contains valuable information for building a kernel if a pre-built
kernel with Coda support (or loadable coda kernel modules) cannot be used.

## Installing and Configuring the Coda Client Binaries and Documentation

=== "Linux"

    Install RPMS using following commands.

    ``` sh
    # rpm -Uvh lwp-<release>.i386.rpm
    # rpm -Uvh rvm-<release>.i386.rpm
    # rpm -Uvh rpc2-<release>.i386.rpm
    # rpm -Uvh coda-debug-client-<release>.i386.rpm
    ```

=== "FreeBSD"

    Build and install coda5_client port from `/usr/ports/net/coda5_client`.

    ``` sh
    # cd /usr/ports/net/coda5_client
    # make
    # make install
    ```

=== "NetBSD"

    Build and install coda5_client source package from
    `/usr/pkgsrc/net/coda5_client`.

    ``` sh
    # cd /usr/pkgsrc/net/coda5_client
    # make
    # make install
    ```

=== "Windows 95/98"

    Install Coda client by running ??????.

Currently, client files for the supported platforms are installed into the
following directories:

=== "Linux"

    - `/usr/bin` - user applications
    - `/usr/sbin` - system binaries
    - `/etc` - other files

=== "FreeBSD"

    - `/usr/local/bin` - user applications
    - `/usr/local/sbin` - system binaries
    - `/usr/local/etc` - other files

=== "NetBSD"

    - `/usr/local/bin` - user applications
    - `/usr/local/sbin` - system binaries
    - `/usr/local/etc` - other files

=== "Windows 95/98"
    - All executables can be found in the directory `C:\usr\coda\bin`.

!!! note

    Please make sure your PATH environment variable is set properly for your
    platform.  That is, ensure FreeBSD has `/usr/local/{bin,sbin}` and NetBSD
    has `/usr/pkg/{bin,sbin}` in its default search paths.  Linux installs Coda
    into the default system paths, so Linux should not need its default search
    path modified.

## Linux and the BSD's: running venus, the client cache manager

Once the binaries and documentation are installed, configuration of venus is
identical for all platforms when **venus-setup** is used.  To run the script,
type:

    venus-setup comma,separated,list,of,servers cache_size_in_kilobytes

!!! note

    **venus-setup** is not needed when you install from the Debian/Ubuntu
    binary packages because its functionality has been integrated into
    **dpkg-reconfigure**.

The **venus-setup** script does all the hard work, it will setup the coda
control files, create `/dev/cfs0` to communicate with the kernel, &hellip;  It
also initializes a directory for cache files.  In your first Coda run we
recommend a small cache, say 20MB. The cache size should be at least 10Meg,
typically 60-200Meg is used.  Do not go above 300Meg.  All the files created
will be placed under `/usr/coda`. You should make sure that there is enough
space in the file system on which `/usr/coda` resides to hold a fully populated
cache.

For example, `venus-setup micky,minnie,goofy 40000` will set up **venus** to
look for the set of servers _micky_, _minnie_, and _goofy_ and configure it to
use 40MB of disk space for caching.  This information is kept in
`/etc/coda/venus.conf`.  We strongly recommend that you initially try your
client against _testserver.coda.cs.cmu.edu_ first, and keep the cache size to
20000.

Start venus with `venus &`. An **-init** flag can be given when **venus**
is started to flush the local cache contents.  Installation procedure performed
by **venus-setup** forces **venus** to inititialize cache upon first startup.
The **-init** flag can be given if Coda cannot recover its cache after a crash
or after re-editing the `venus.conf` file manually.

Observe the venus log with `tail -f /var/log/coda/venus.err` it will tell you
when **venus** has started and give status.  Type `codacon` to see the actions
of the client in more detail, such as the communication between **venus** and
the Coda servers.

It is possible to see the upcalls from the kernel to **venus** by turning up
logging, but they are not very interesting.  (To turn on minimal debugging,
type `vutil -d 1` and then `tail -f /var/log/coda/venus.log`.)

To halt venus, type:

    # umount /coda  # Linux only
    # vutil shutdown

Or you can kill -9 venus, if you must.

!!! note "Linux specific notes"

    - Before restarting venus, `/coda` must be unmounted.  If umounting `/coda`
      gives trouble, make sure to exit all process that hang on to the Coda
      filesystem, e.g. by having files open or being cd'd into /coda.  A utility
      like **lsof** and **fuser** can help with this.

    - `/proc/fs/coda` has interesting Coda statistics.

    -  You can enable kernel debugging  with `vutil -kdebug 4095` and call tracing
       with `vutil ktrace 1`. The messages appear in `/var/log/messages`.

## Windows 95: Starting and Configuring a Coda client

During installation you will be prompted for the IP address of your Coda
server(s).  Enter this as indicated.

### CodaStart

**CodaStart** is a Win32 windows-based application to control and observe the
Windows 95 Coda client. It will be enhanced in the future. For now it provides
a convenient way to start **venus.exe**.  It also displays the kernel-venus
communication for debugging purposes.  Printing the messages can be stopped by
unticking the _Monitor_ check box. The _Reset_ button clears the display.

When you start venus for the first time, or you want to reinitialize its cache,
tick the _Init venus_ check box. This will start venus with the **-init** and
**-cf 1500** flag set. To add or override flags use the _Configure_ button.
Start venus by clicking the left _Start_ button. The 'Status' message will tell
you 'Running' when **venus** starts.

Coda will be mounted automatically on the drive specified in the
`C:\usr\coda\etc\vstab` file. `vstab` is created by the installer.

To unmount, type **cfs uk** in a DOS window. This will cleanly shut down venus
as well.

You are now ready to browse through the Coda filesystem using the explorer!

!!! important

    In some installations the DPMI DOS Extender window suspends when it is not
    active. In this case untick the window property
    _Properties_->_Misc_->_Background_->_Always Suspend_. If it is unticked,
    ticking and unticking it again might help. Also untick the _Termination_
    flag, to allow Coda to automatically shutdown, when the system shuts down.
    For your convenience tick the _Close on Exit_ check box in the _Program_
    tab.

## Upgrading from a previous Release

If you are upgrading Coda from a previous release, do not run **venus-setup**
again!  Simply obtain the packages for your platform as indicated above.  Then,
shutdown **venus** by becoming root and issue the following command:

    # vutil -shutdown

Next, install the binary packages following the installation instructions
above.  Make sure you have the appropriate kernel or module for \*BSD systems
or the correct module for Linux systems.  The `INSTALL.<platform>` document
available from the Coda ftp site will contain relevant notes about upgrading
your system core components.

Once the new binaries are installed and the any necessary kernel updates are
completed, you need to start the new venus for the first time by becoming root
and typing:

    # venus -init &

The **-init** flag is used to re-initialize the `/usr/coda/venus.cache/`.

## Configuration changes made by venus-setup

This section describes the changes that must be made to various configuration
files.  Samples of the files are contained in [Example Files](examples.md).

**/etc/coda/venus.conf**
The Coda client configuration file `venus.conf` in `/etc/coda` is copied from
`/etc/coda/venus.conf.ex` if it doesn't exist yet. All configurable options in
this file are extensively documented by comments in the file itself.

**/etc/rc.***
To automatically start venus at boot time, the script **rc.venus** is provided
in `/usr/local/etc` for FreeBSD systems and `/usr/pkg/etc` for NetBSD.
**venus.init** is provided for RedHat Linux in `/etc/rc.d/init.d`.

=== "Linux"

    RedHat Linux users should create the following symbolic link if it doesn't
    already exist:

        # ln -sf /etc/rc.d/init.d/venus.init /etc/rc.d/rc3.d/S97venus.init

    This will allow **init** to process the file and start venus at boot time.

=== "FreeBSD"

    <!-- this should be fixed by proper port -->
    FreeBSD system users should create `/usr/local/etc/rc.d/venus.sh` (with
    _rwxr-xr-x_ permissions), containing:

    ``` sh
    #!/bin/sh
    echo -n " venus"
    /usr/local/sbin/venus &
    ```

=== "NetBSD"
    <!--Same here -->

    NetBSD system users should create `/usr/pkg/etc/rc.d/venus.sh` (with
    _rwxr-xr-x_ permissions), containing:

    ``` sh
    #!/bin/sh
    echo -n " venus"
    /usr/pkg/sbin/venus &
    ```

**/etc/services**
The following port numbers are added to `/etc/services` by **venus-setup**:

    coda_opcons     1355/udp                        # Coda opcons
    coda_auth       1357/udp                        # Coda auth
    coda_udpsrv     1359/udp                        # Coda udpsrv
    coda_filesrv    1361/udp                        # Coda filesrv
    coda_venus      1363/udp                        # Coda venus
    coda_backup     1407/tcp                        # coda backup service
    codacon         1423/tcp        venus.cmu       # Coda Console
    coda_aux1       1431/tcp                        # coda auxiliary service
    coda_aux1       1431/udp                        # coda auxiliary service
    coda_aux2       1433/tcp                        # coda auxiliary service
    coda_aux2       1433/udp                        # coda auxiliary service
    coda_aux3       1435/tcp                        # coda auxiliary service
    coda_aux3       1435/udp                        # coda auxiliary service

**/dev/cfs0**
The following major and minor numbers are used for the various platforms:

=== "Linux"
      - Character-special with Major 67 and minor 0 for Linux

        ``` sh
        # mknod /dev/cfs0 c 67 0
        ```

=== "FreeBSD"
      - Character-special with Major 93 and minor 0 for FreeBSD

        ``` sh
        # mknod /dev/cfs0 c 93 0
        ```

=== "NetBSD"
      - Character-special with Major 46 and minor 0 for NetBSD 1.2

        ``` sh
        # mknod /dev/cfs0 c 46 0
        ```

      - Character-special with Major 51 and minor 0 for NetBSD 1.3

        ``` sh
        # mknod /dev/cfs0 c 51 0
        ```

Please be sure to check `INSTALL.<platform>` to make sure these numbers have
not changed.  For Linux and FreeBSD, the above numbers are officially
registered.  Coda on NetBSD at present does not have formally registered major
device numbers so these are subject to change from release to release.

## Where Data and Configuration Information is Stored

- `/usr/coda/etc` - Coda configuration files critical for venus startup.
- `/usr/coda/venus.cache` - Cached copies of Coda files.
- `/usr/coda/spool` - Saved CMLs from automatic or manual checkpointing.
- `/usr/coda/tmp` - Temporary hoard related files.
- `/etc/services` - Various port numbers used by Coda for client and server communication.
