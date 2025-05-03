---
title: CODASRV(8)
footer: Coda Distributed File System
---

## NAME

codasrv - Coda file system server

## SYNOPSIS

**codasrv** \[-d debug\_level] \[-rpcdebug rpc2debuglevel]
\[-p number\_of\_processes] \[-l large\_vnodes] \[-s small\_vnodes]
\[-k stack\_size] \[-w wait\_interval] \[-r RPC\_retry\_count]
\[-o RPC\_timeout] \[-c check\_interval] \[-t RPC trace buffers]
\[-forcesalvage] \[-quicksalvage] \[-rvm logdevice datadevice datalength]
\[-nores] \[-trunc percent] \[-nocmp] \[-nopy] \[-nodumpvm]
\[-nosalvageonshutdown] \[-mondhost hostname] \[-mondportal port]
\[-debarrenize] \[-optstore] \[-rvmopt] \[-newchecklevel checklevel]

## DESCRIPTION

**codasrv** is the Coda File System file server. It services requests from
client machines' **venus** processes and maintains the file system.

**codasrv's** command line options are:

**-d** *debug_level*

:   Sets the internal debugging level to *debug level*. This controls
    the amount of debugging output **codasrv** will generate.

**-p** *number_of_processes*

:   Sets the number of light weight processes that will be used to handle
    concurrent requests.

**-l** *large_vnodes*

:   number of large vnodes in lru cache

**-s** *small_vnodes*

:   number of small vnodes in lru cache

**-k** *stack_size*

:   stack size of LWP threads in Kbytes

**-w** *wait_interval*

:   interval for CallBackCheckLWP to see which clients are alive

**-r** *RPC_retry_count*

:   number of times a call is retried before reporting death

**-o** *RPC_timeout*

:   default timeout for all rpcs

**-c** *check_interval*

:   The interval at which shutdown is checked

**-t** *RPC_trace_buffers*

:   number of entries in the rpc trace buffer

**-forcesalvage**

:   force a full salvage on all volumes

**-quicksalvage**

:   salvage only the headers of the volumes - currently it always does a full salvage

**-rvm** *logdevice* *datadevice* *length*

:   The **-rvm** switch allows you to specify the location of the log
    device, data device, and length of the recoverable segment used by the
    server. This parameter is not optional.

**-nores**

:   Turn off resolution on the servers

**-trunc** *percent*

:   Specify how full the log can get before it is truncated. Default is 50%

**-nocmp**

:   Directory replica contents will not be checked for equality at end of
    resolution

**-nopy**

:   Suppress polling and yielding by threads that run for a long period

**-nodumpvm**

:   Do not dump in-memory copy of recoverable segment before shutdown

**-nosalvageonshutdown**

:   Do not salvage volumes on shutdown

**-mondhost** *hostname*

:   Host where *mond* database is stored

**-mondportal** *port*

:   Port to use when contacting the *mond* host.

**-debarrenize**

:   Create an inode for a vnode if the inode cannot be found by the salvager

**-opstore**

:   Optimized Stores - return to the client before sftp completes

**-rvmopt**

:   Turn on inter/intra transaction rvm optimizations

**-newchecklevel** *checklevel*

:   Set the level of checking done by new plumber.

## FILES

- `/vice/srv/SrvErr`
- `/vice/srv/SrvLog*`

## SEE ALSO

**startserver**(8)

## AUTHOR

- Joshua Raiff, 1993, Created man page
