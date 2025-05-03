# TroubleShooting

The Coda filesystem is still under development, and there certainly are several
bugs which can crash both clients and servers. However, many problems users
observe are related to semantical differences of the Coda filesystem compared
to well-known NFS or SMB network filesystems.

This chapter will point out several logs to look at for identifying the cause
of problems. Even if the source of the problem cannot be found, the information
gathered from Coda's logging mechanisms will make it easier for people on the
coda mailinglist <mailto:codalist@coda.cs.cmu.edu> to assist in solving the
problem(s).

Some of the more common problems are illustrated in detail. At the end of this
chapter some of the more involved debugging techniques will be addressed. This
will be helpful to developers to isolate problems more easily.  At the end
there is a whole section describing how to solve some problems with using Coda
client on Windows95.

## Basic Troubleshooting

Most problems can be solved, or at least recognized by using the information
logged by the clients and servers. The first step in finding out where the
problems stems from is doing a **tail -f** on the logfiles.

It must also be noted that, when coda clients and servers crash they do not
`dump core', but start sleeping so that we developers can attach debuggers.
As a result, a crashed client or server still shows up in the **ps auxwww**
output, and only the combination of lack of file-service and error messages in
logfiles indicate that something is really wrong.

Since release 5.3.4 servers now actually exit when they crash, create a file
`/vice/srv/ZOMBIFY` to force a server to go into an endless sleep again.

### Client debugging output

- **codacon** is a program which connects to venus and provides the user with
  run-time information. It is the initial source of information, but cannot be
  used to look back into the history. It is therefore advisable to always have
  a codacon running in a dedicated xterm.

  ``` sh
  client$ codacon
  ```

- `/usr/coda/etc/console` is a logfile which contains mostly error or warning
  messages, and is a place to look for errors which might have occured. When
  assertions in the code fail, it is logged here.

- `/usr/coda/venus.cache/venus.log` contains more in-depth information about
  the running system, which can be helpful to find out what the client is or
  was doing.

### Server logs

- **cmon** is an ncurses program that can be run on a client to gather and
  display statistics from a group of servers. When a server goes down it will
  not respond to the statistics requests, which makes this a simple method for
  monitoring server availability.

  ``` sh
  client$ cmon server1 server2 server3 ...
  ```

- `/vice/srv/SrvLog` and `/vice/srv/SrvErr` are the server logfiles.

Other logfiles that could be helpful in discovering problems are:

- `/vice/auth2/AuthLog`
- `/vice/srv/UpdateClntLog`
- `/vice/srv/UpdateSrvLog`

## Client Problems

!!! question "Client does not connect to `testserver.coda.cs.cmu.edu`."

    When you have set up your client for the first time, and it can not connect
    to the testserver at CMU, there are a couple of possible reasons. You might
    be running an old release of Coda, check the Coda web-site to see what the
    latest release is.

    Another common reason is that your site is behind a firewall, which blocks,
    or allows only outgoing, udp traffic. Either try Coda on a machine outside
    of the firewall, or set up your own server.

    The third reason is that the testserver might be down, for maintenance or
    upgrades. That does not happen often, but you can check whether it is up,
    and how long it has been running using **cmon**.

    ``` sh
    client$ cmon testserver.coda.cs.cmu.edu
    ```

!!! question "**venus** comes up but prints "cannot find RootVolume"."

    All of the reasons in the previous item could be the cause.  It is also
    possible that your `/etc/services` file is not allright.  It needs the
    entries:

    ```
    # IANA allocated Coda filesystem port numbers
    codaauth2       370/tcp
    codaauth2       370/udp    # Coda authentication server
    venus           2430/tcp   # codacon port
    venus           2430/udp   # venus callback/wbc interface
    venus-se        2431/tcp   # tcp side effects
    venus-se        2431/udp   # udp sftp side effect
    codasrv         2432/tcp   # not used
    codasrv         2432/udp   # server port
    codasrv-se      2433/tcp   # tcp side effects
    codasrv-se      2433/udp   # udp sftp side effect
    ```

!!! question "Trying to access a file returns "Connection timed out (ETIMEDOUT)"."

    The main reason for getting "Connection timed out" errors is that the
    volume where the file is located is disconnected from the servers. However,
    it can also occur in some cases when the client is in write-disconnected
    mode, and there is an attempt to read a file which is open for writing. See
    [Disconnections](#disconnections) for more information.

!!! question "Commands do not return, except by using _^C_."

    When command are hanging it is likely that **venus** has crashed. Check
    `/usr/coda/etc/console` and `/usr/coda/venus.cache/venus.log`.

!!! question "**venus** fails when restarted."

    If venus complains in `venus.log` about not being able to open `/dev/cfs0`,
    it is because `/coda` is still mounted.

    ``` sh
    # umount /coda
    ```

    Another reason for not restarting is that another copy of venus is still
    around, and venus is unable to open it's network socket. In this case there
    will be a message in `venus.log` stating that RPC2_CommInit has failed.

!!! question "**venus** doesn't start."

    A reason is that you do not have the correct kernel module. This can be
    tested by inserting the module by hand, and then listing the available
    modules. _coda_ should show up in that listing. Otherwise reinstall (or
    recompile) a new module.

    ``` sh
    # depmod -a
    # modprobe coda
    # lsmod
    Module                  Size  Used by
    coda                   50488   2
    ...
    ```

    If the kernel module can be loaded without errors, check `venus.log`. A
    message "Cannot get rootvolume name" indicated either a misconfigured
    server or the _codasrv_ and/or _codasrv-se_ ports are not defined in
    `/etc/services`, which should contain the following entries.

    (See above for the entries needed.)

!!! question "I'm disconnected and **venus** doesn't start."

    Put the hostnames of your servers in `/etc/hosts`.

<!-- That is not a nice answer, commenting it out. -JH
!!! question "I cannot get tokens while disconnected."

    Take a vacation until we release a version of Coda which uses it's
    telepathic abilities to contact the **auth2** server. We will add this
    feature.
-->

!!! question "Hoard doesn't work."

    Make sure you have version 5.0 of Coda or later.  Before you can hoard you
    must make sure that:

      - You started **venus** with the flag **-primaryuser &lt;youruid&gt;**
      - You have tokens

!!! question "File System Full."

    You may get a message of the form, "/usr: file system full" when you have
    not put any extra data on that disk.  If the `venus.cache` directory is on
    that disk, that is probably your problem.  You can execute either **cfs
    flushobject &lt;object&gt;** or **cfs flushvolume &lt;dir&gt;** to try to
    free up some disk space.

    !!! warning

        Be careful when flushing anything while disconnected.  You will lose
        any changes that you made.

    In the long term, the cache size in `/usr/coda/etc/vstab` is probably too
    large for the disk and should be made smaller.

## Server Problems

!!! question "The server crashed and prints messages about "AllocViaWrapAround"."

    This happens when you have a resolution log that is full.  In the `SrvLog`
    you will usually be able to see which volume is affected, take down it's
    volume id (you may need to consult `/vice/vol/VRList` on the SCM to do
    this.  Kill the dead (zombied) server, and restart it.  The moment it is up
    you do:

    ``` sh
    # filcon isolate -s this_server   # Disconnect clients
    # volutil setlogparms <volid> reson 4 logsize 16384
    # filcon clear -s <this_server>
    ```

    Unless you do *huge* things 16k should be plenty.

!!! question "Server doesn't start due to salvaging problems."

    If this happens you have several options.  If the server has crashed during
    salvaging it will not come up by trying again, you must either repair the
    damaged volume or not attach that volume.

    Not attaching the volume is done as follows.  Find the volume id of the
    damaged volume in the `SrvLog`. Create a file named `/vice/vol/skipsalvage`
    with the number of volumes listed followed by the hexadecimal volume ids.

    ```
    1
    0xdd000123
    ```

    In this instance it indicates that a single volume is to be skipped,
    followed by the volume id of the replica that should not be attached.  If
    this volume is a replicated volume, take all replicas offline, since
    otherwise the clients will get very confused.

    You can also try to repair the volume with **norton**.  **norton** is
    invoked as: `norton <LOG> <DATA> <DATA-SIZE>`.

    These parameters can be found in `/etc/coda/server.conf`.
    See [norton.8](../manpages/norton.8.md) for detailed information about
    **norton**'s operation. Built-in help is also available while running
    **norton**.

    !!! note

         1. Often corruption is replicated.  This means that if you find a
            server has crashed and does not want to salvage a volume, your
            other replicas may suffer the same fate: the risk is that you may
            have to go back to tape (you do make tapes, right?).  Therefore
            *first* copy out good data from the available replicas, *then*
            attend to repairing or skipping them in salvage.

         1. Very often you have to take both a volume and its most recent clone
            (generated during backup) offline, since corruption in a volume is
            inherited by the clone.

!!! question "How to restore a backup from tape."

    <!-- Change into less fairy-tale fashioned -->
    Tuesday I have lost my email folder - the whole volume `moose:braam.life`
    was corrupted on server _moose_ and it wouldn't salvage.  Here is how I got
    it back.

     1. First I tried mounting `moose.braam.life.0.backup` but this was
        corrupted too.

     1. On the SCM in `/vice/vol/VRList` I found the replicated volume number
        _f0000427_ and the volume number _ce000011_ (fictious) for the volume.

     1. I logged in as root to _bison_, our backup controlller.  I read the
        `backuplog` for Tuesday morning in `/vice/backuplogs/backuplog.DATE`
        and saw that the incremental dump for August 31st had been fine.  At
        the end of that log, I saw the name _f0000427.ce000011_ listed as
        dumped under `/backup` (a mere symlink) and `/backup2` as spool
        directory with the actual file. The backup log almost shows how to move
        the tape to the correct place and invoke restore:

        ``` sh
        # cd /backup2
        # mt -f /dev/nst0 rewind
        # restore -b 500 -f /dev/nst0 -s 3 -i
        Value after "-s" depends upon which /backup<123]> volume we pick to
        restore backup.
        restore> cd 31Aug1998
        restore> add viotti.coda.cs.cmu.edu-f0000427.ce000011
        restore> extract
        Specify volume #: 1
        ```

     1. In `/vice/db/dumplist` I saw that the last full backup had been on
        Friday Aug28. I went to the machine room and inserted that tape (recent
        tapes are above _bison_).  This time _f0000427.ce000011_ was a 200MB
        file (the last full dump) in `/backup3`. I extract the file as above.

     1. Then I merged the two dumps:

        ``` sh
        # merge /restore/peter.mail \
            /backup2/28Aug1998/f0000427.ce000011 \
            /backup3/31Aug1998/f0000427.ce000011
        ```

     1. This took a minute or two to create `/restore/peter.mail`. Now all that
        was needed was to upload that to a volume:

        ``` sh
        # volutil -h moose restore /restore/peter.mail /vicepa vio:braam.mail.restored
        ```

     1. Back to the SCM, to update the volume databases:

        ``` sh
        # bldvldb.sh viotti
        ```

     1. Now I could mount the restored volume:

        ``` sh
        # cfs mkm restored-mail vio:braam.mail.restored
        ```

        and copy it into a read write volume using cpio or tar.

!!! question "createvol_rep reports RPC2_NOBINDING."

    When trying to create volumes, and createvol_rep reports "RPC2_NOBINDING",
    it is an indication that the server is not (yet) accepting connections.

    It is useful to look at `/vice/srv/SrvLog`, the server performs the
    equivalent of **fsck** on startup, which might take some time. Only when
    the server logs "Fileserver Started" in `SrvLog`, it starts accepting
    incoming connections.

    Another reason could be that an old server is still around, blocking the
    new server from accessing the network ports.

!!! question ""RPC2_DUPLICATESERVER" in the auth2 logs."

    Some process has the UDP port open which **auth2** is trying to obtain. In
    most cases this is an already running copy of **auth2**.  Kill all running
    copies of the program in question and restart them.

!!! question "Server crashed shortly after updating files in `/vice/db`."

    Servers can crash when they are given inconsistent or bad data-files. You
    should check whether **updateclnt** and **updatesrv** are both running on
    the SCM and the machine that has crashed. You can kill and restart them.
    Then restart **codasrv** and it should come up.

!!! question "Users cannot authenticate or created volumes are not mountable."

    Check whether **auth2**, **updateclnt**, and **updatesrv** are running on
    all fileservers.  Also check their logfiles for possible errors.

## Disconnections

As most common problems are related to the semantical differences arising as a
result of _involuntary_ disconnections, this section contains some background
information of why volumes become disconnected or write-disconnected. And how
to get them to reconnect again.

!!! question "Volume is fully disconnected."

    There are several reasons why a coda client may have disconnected some or
    all volumes from an accessible server.

      - **Pending reintegration.**

        When modifications have been made to the volume in disconnected mode, the
        client will not reconnected the volume until all changes have been
        reintegrated. Also, reintegration will not occur without proper user
        authentication tokens. Furthermore, reintegration is suspended as long as
        there are objects in conflict.

        The most important item here is to have a **codacon** process running,
        since it will give up-to-date information on what venus is doing. Venus
        will inform the user about missing coda authentication tokens,
        "Reintegration: pending tokens for user &lt;uid&gt;".  In this case the
        user should authenticate himself using the **clog** command.

        Conflicts, which require us to use the **repair** tool, are conveyed using
        the "local object &lt;pathname&gt; inconsistent" message. Otherwise
        **codacon** should show messages about _backfetches_, and how many
        modifications were successfully reintegrated.

      - **Access permissions.**

        The client may also disconnect when a servers reports an error to an
        operation, when according to the client this is a valid operation. Causes
        for this are authentication failure; check tokens using **ctokens** and
        optionally obtain new tokens using **clog**. Or inconsistencies between the
        data cached on the client and the actual data stored on the server; this
        will reveal itself as an inconsistent object during subsequent
        reintegration.

      - **Lost connections.**

        Sometimes the client does not receive a prompt reply from an accessible
        server, and marks the server as dead. This will ofcourse disconnect the
        volume if the last server is lost. Once every five minutes, the client
        automatically verifies connectivity with all known servers, and can thus
        recover from lost connections. However, this action can also be triggered
        by the user by excecuting the **cfs checkservers**.  If **cfs
        checkservers** reports that servers are unreachable, it might be
        interesting to check with **cmon**, if the server is responding at all,
        since we might be faced with a crashed server. When a server was considered
        unreachable, but is successfully contacted after **cfs checkservers**,
        reintegration will automatically start (when a user has tokens, and there
        are no inconsistencies).

!!! question "Volume is write-disconnected."

    *Write-disconnected operation* is used as often as **weakly connected
    mode** to describe this volume state, and they are effectively the same.
    This is the special situation where a client observes a weak connectivity
    with a server, and therefore forces the associated volumes in weakly
    connected mode.  Weakly connected volumes postpone writing to the server to
    significantly reduce waiting on a slow network connection. Read operations
    are still serviced by the local cache and the servers, as in fully
    connected mode. Which is why this mode of operation is also called
    write-disconnected operation.

    The write operations are effectively a continuous reintegration
    (*trickle-reintegration*) in the background. This mode, therefore,
    requires users to be authenticated and gives more chance for possible file
    conflicts.  The following points are several reasons for write-disconnected
    operation.

      - **Weak network connectivity.**

        Venus uses bandwidth estimates made by the RPC2 communication layer to
        decide on the quality of the network connection with the servers. As soon
        as the connectivity to one of the servers drops to below the weakly
        connected treshhold (currently 50 KB/s), it will force all volumes
        associated with that server into weakly-connected mode. The **cfs wr** can
        be used to force the volumes back into fully connected mode, and
        immediately reintegrate all changes.

        To avoid switching to weakly connected mode, use **cfs strong**. This way
        **venus** ignores bandwidth estimates. **cfs adaptive** will make **venus**
        revert to interpreting bandwidth estimates.

        When the user was not authenticated, or conflicts were created during the
        write-disconnected operation, the user must first obtain proper
        authentication tokens or repair any inconsistent objects before the volume
        becomes fully connected again. Here again **codacon** is an invaluable tool
        for obtaining insight into the client's behaviour.

      - **User requested write-disconnect mode.**

        Users can ask venus to force volumes in write-disconnected mode,
        exchanging high consistency for significantly improved performance. By
        using the **-age** and **-time** flags in the **cfs wd** commandline,
        some control is given about the speed at which **venus** performs the
        trickle-reintegration. For instance, to perform the trickle-reintegrate
        more quickly than the default, where only mutations to the filesystem
        older than 15 minutes are reintegrated. You could use
        **cfs wd -age 5**, which will attempt to reintegrate all mutations
        older than 5 seconds.

      - **Pending reintegration.**

        When a volume is write-disconnected, it will stay write-disconnected until
        a user properly authenticates using **clog**.

## Advanced Troubleshooting

!!! question "Troubleshooting network problems with rpc2tcpdump."

    **rpc2tcpdump** is the regular tcpdump, which is modified to decode rpc2
    protocol headers. This makes it a very useful tool for analyzing why
    programs fail to work.

    All traffic between **venus** and the Coda servers can be viewed using the
    following command:

    ``` sh
    # tcpdump -s120 -Trpc2 port venus or port venus-se
    ```

    To identify problems with **clog**, for instance which server it is trying
    to get tokens from.

    ``` sh
    # tcpdump -s120 -Trpc2 port codaauth
    ```

!!! question "Debugging with gdb."

    To be able to debug programs that use RVM, most coda related application
    will go into an endless sleep when something goes really wrong. They print
    their process-id in the log (f.i. `venus.log` or `SrvLog`), and a user can
    attach a debugger to the crashed, but still running, program.

    ``` sh
    # gdb /usr/sbin/venus $(pidof venus)
    ```

    This makes it possible to get a stack backtrace (**where**), go to a
    specific stack frame (**frame &lt;x&gt;**), or view the contents of
    variables, (**print &lt;varname&gt;**). By installing the coda sources in
    same place as where the binaries were initially built from, it is possible
    to view the surrounding code fragment from within the debugger using the
    **list** command.

    When using RedHat Linux rpms, you can install the sources in the right
    place by installing the coda source rpm file.

    ``` sh
    # rpm -i coda-x.x.x.src.rpm
    # rpm -bp /usr/src/redhat/SPECS/coda.spec
    ```

    On other platforms look at the paths reported in the backtrace and unpack
    the source tarball in the correct place.

    ``` sh
    (gdb) where
    #0  CommInit () at /usr/local/src/coda-5.3.6/coda-src/venus/comm.cc:175
    #1  0x80fa8c3 in main (argc=1, argv=0xbffffda4)
        at /usr/local/src/coda-5.3.6/coda-src/venus/venus.cc:168
    (gdb) quit
    # cd /usr/local/src
    # tar -xvzf coda-5.3.6.tgz
    ```

## Troubleshooting on Windows 95

### Common problems

!!! question "Unable to shutdown Windows95."

    Check the DOS Windows settings of **Venus**. The check box in
    _Properties_->_Misc_->_Termination_ must be unticked.

!!! question "Cannot reboot Windows, I think it is due to the Coda VXDs."

    Boot your System in DOS mode by pressing ++f8++ on boot time. **cd** to the
    Windows directory and type **edit system.ini**. In the section _[enh386]_
    you will find following entries:

    ```
    device=c:\usr\coda\bin\mmap.vxd
    device=c:\usr\coda\bin\mcstub.vxd
    ```

    Comment them out by using a _;_ in front of the lines. Try to restart
    Windows again.

!!! question "How can I find out why `venus.exe` crashed."

    See [Client Troubleshooting](#client-problems). When this happens it might
    not be possible to restart Venus, if it is still mounted. In this case try
    to unmount by typing **unmount &lt;drive&gt;**. If it does not work, you
    want to reboot the machine.

!!! question "How can I find out more about what has happened."

    Look in the file `c:\vxd.log`. The file system driver **codadev.vxd**
    prints information about all requests and answers in this file. the
    information is only stored if the debug level has been turned on. the debug
    level is specified in the registry key
    _HKLM/System/CurrentControlSet/Services/VxD/Codadev/Debuglevel_. Set the
    debug level higher than _0_ to receive messages in the debug file.

!!! question "I hook my running machine off the network and the explorer blocks."

    **Venus** switches to disconnected mode after a short timeout. After that
    it should work fine. If it doesn't, check if you have 'network connections' set
    up in the explorer (e.g. Windows shares). Sharing such a network drive
    blocks your system when no network is available because the SMB share does
    not handle disconnected operation.

### Restrictions

- Most command line tools, that talk to **Venus** through the ioctl interface
  of the Coda kernel module seem to work even when they print error messages.
- Handling large files (in particular executables) does not work well in a low
  bandwidth scenario.
- **cfs.exe** and **hoard.exe** use absolute pathnames so far.
- Short filenames are not supported under DOS environment yet. You can access
  files, but you need to use the long filenames.
