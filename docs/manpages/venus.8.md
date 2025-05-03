---
title: VENUS(8)
footer: Coda Distributed File System
---

## NAME

venus - Coda client cache manager

## SYNOPSIS

**venus** \[-k kernel device] \[-cf cache files] \[-c cache blocks]
\[-mles CML entries] \[-d debuglevel] \[-rpcdebug rpc2 debuglevel]
\[-f cache directory] \[-m COP modes] \[-console console file]
\[-retries RPC2 retries] \[-timeout RPC2 timeout] \[-ws SFTP window size]
\[-sa SFTP sendahead] \[-ap SFTP ackpoint] \[-init] \[-hdbes hoard entries]
\[-rvmt RVM type] \[-maxprefetchers fetch threads]
\[-maxworkers worker threads] \[-maxcbservers callback threads]
\[-vld RVM log device] \[-vlds RVM log size] \[-vdd RVM data device]
\[-vdds RVM data size] \[-rdscs RVM data chunk size]
\[-rdsnl RVM data nr lists] \[-logopts 0 | 1] \[-swt weight] \[-mwt weight]
\[-ssf scale factor]

## DESCRIPTION

**venus** manages a cache of files and directories for a client
workstation. It has a host of parameters and configuration options.
Default values of everything are compiled into **venus**. Some of these
are overridden by the values in the `venus.conf` file, provided that
it exists. Both default and venus.conf values may be overridden with
command-line arguments. Venus must be run as root.

The command-line options are:

**-k**

:   Take *kernel device* to be the device for kernel/venus
    communication.

Default: `/dev/cfs0`

**-cf**

:   Limit the size of the file cache to *cache* files entries.

**-c**

:   Limit the size of the file cache to *cache* blocks 1K blocks.

**-mles**

:   Number of Client Modification Log entries.

**-vols**

:   Limit the size of the volume cache to *volumes* entries.

**-vsgs**

:   Limit the size of the VSG cache to *vsgs* entries.

**-d**

:   Initialize the debug level to *debug* level.

Default: **0**

**-f**

:   Take *cache directory* to be the directory for the file, volume, and
    VSG caches. On some platforms **venus** garbage collects any files
    it doesnt recognize in the cache directory so use caution when
    supplying this argument.

Venus will create the directory if it doesnt already exist. The
directory should have mode bits of `rwx------` to protect the
cache from malicious local users.

Default: **/usr/coda/venus.cache**

**-m**

:   Controls what Coda Optimistic Protocol (COP) options are enabled.
    *COP modes* is interpreted according to the following bit-mask: \[
    PIGGYCOP2 | ASYNCCOP2 | ASYNCCOP1 ]. Only some combinations are
    legal.

Default: **\[ PIGGYCOP2 | ASYNCCOP2 \]**

**-console**

:   Redirects console messages to *console* file.

Default: `/dev/console`

**-retries**

:   Sets the number of RPC2 retries to *RPC2* retries.

Default: **5**

**-timeout**

:   Sets the RPC2 timeout period to *RPC2 timeout* seconds.

Default: **15**

**-ws**

:   Sets the SFTP window size to *SFTP window* size packets.

Default: **8**

**-sa**

:   Sets the SFTP send ahead to *SFTP send* ahead packets.

Default: **4**

**-ap**

:   Sets the SFTP ack point to *SFTP ack* point packets.

Default: **4**

**-init**

:   Initializes (i.e., clears) file, volume, and VSG caches.

**-hdbes**

:   Number of hoard database entries.

**-rvmt**

:   Media that RVM resides on. Meaningful values are: 2 (use a file to
    persistently back RVM memory) and 3 (do not use a file as RVM
    backing store, no persistency).

**-maxprefetchers**

:   Maximum number of threads doing prefetch ioctls.

**-maxworkers**

:   Number of worker threads.

**-maxcbservers**

:   Number of callback server threads.

**-vld**

:   Location of the venus RVM log file or partition.

Default: `/usr/coda/LOG`

**-vlds**

:   Size of the log file or partition.

**-vdd**

:   Location of the venus RVM data file or partition.

Default: `/usr/coda/DATA`

**-vdds**

:   Size of the RVM data file or partition.

**-rdscs**

:   Specify RDS chunk size.

**-rdsnl**

:   Number of RDS nlists.

**-logopts**

:   Turn on log optimization.

**-swt**

:   Short term cache priority weight.

**-mwt**

:   Medium term cache priority weight.

**-ssf**

:   Short term cache scale factor.

## DIAGNOSTICS

Venus writes debugging information into the file `<cache directory>/venus.log`.
The verbosity of this output is controlled by the *debug* level parameter. High
priority messages are also written to the console (which may be redirected with
the console option at start-up).  Fatal errors will cause the internal state of
venus to be dumped to the log file, and a core file to be left in
`<cache directory>/core`.

Venus writes its process id into the file `<cache directory>/pid`. The
**vutil**(8) program reads the pid file and dynamically alter Venus
behavior by sending signals to it.

Venus may be unable to unmount itself cleanly when it exits. Usually
this is due to processes which have references to vnodes in the Coda
namespace (e.g., a process is cd'ed somewhere in Coda). Once these
references are released, `/coda` can be unmounted.

## SEE ALSO

**venus.conf**(5), **vutil**(8)

## AUTHORS

- Jay Kistler, 1990, Created
- Joshua Raiff, 1993, Documented added switches
