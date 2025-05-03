# System Overview

Each Coda client sees the Coda File System as a single tree, `/coda`.  In
reality this tree is an illusion supported by _System Control Machine (SCM)_,
several dedicated file servers, and a local area network.  One of the servers
may double as the (SCM).  Figure 2 illustrates the server organization.

_Figure 2: Typical Coda server organization_

``` mermaid
architecture-beta
  group server1(server)[Coda Server]
  service file1(disk)[File Server] in server1
  service auth1(database)[Readonly Authentication Server] in server1
  service update1(disk)[Update Client] in server1
  service kernel1(server)[Kernel] in server1

  group scm(server)[SCM Coda Server]
  service file(disk)[File Server] in scm
  service auth(database)[Read Write Authentication Server] in scm
  service update(disk)[Update Server] in scm
  service kernel(server)[Kernel] in scm

  group server2(server)[Coda Server]
  service file2(disk)[File Server] in server2
  service auth2(database)[Readonly Authentication Server] in server2
  service update2(disk)[Update Client] in server2
  service kernel2(server)[Kernel] in server2

  group client(server)[Coda Client]
  service venus(server) in client

  venus:T -- B:file
  venus:L -- R:file1
  venus:R -- L:file2
```

## Machines

The Coda architecture recognizes three types of machines, _clients_, _servers_
and a _system control machine (or SCM)_. Client machines are typically
single-user workstations used to access shared information.  These machines are
not trusted by servers except for the duration of a login session by an
authenticated user. Server machines are secure, trusted machines whose purpose
is to service client requests for shared data.  As the custodians of shared
information, servers must require authentication of each user before releasing
shared data to the users client workstation.  The third machine type is the
system control machine  (SCM).  The purpose of the SCM is to provide a single
point of control for ease of administration.  Logically, the SCM is distinct
from the servers, but, physically, the SCM can also act as a server.

## Processes

Each of the dedicated file servers must run a number of processes. These
processes are shown in Table 1 and are described below.

_Table 1: Server Processes_

| Process | Description |
| ------- | ----------- |
| File Server | The **codasrv** process interacts with the **venus** cache manager process on clients.  Together they fulfill user requests for shared data stored on the server.   When started, the server process will salvage the file system.   The presence of a file called `SHUTDOWN` in the `/vice/srv` directory indicates that the server process exited normally. |
| Auth Server | The **auth2** process runs on all servers.  It validates user passwords and issues a token for that user if the password is correct.   However, passwords may only be changed at the SCM.  Hence,  the password database is read-only replicated to all servers and the SCM maintains the read-write replica.  Changes to the password file are updated automatically through the **updateclnt**/**updatesrv** processes described below.  On all servers (except the SCM), auth2 is invoked with the **-chk** option which makes it operate in read-only mode. |
| Update Client | The **updateclnt** process works in conjunction with the **updatesrv** process (running on the SCM) to keep read-only copies of system files and databases in sync with their corresponding read-write copy.  The updateclnt process checks with the updatesrv process on the SCM periodically to ensure that the local read-only copy is the latest copy.  Thus, when the read-write copy is updated, the read-only copies will be automatically updated after a short delay. |

Here is a list of the typical processes of a running file server.

``` sh
  PID TT  STAT      TIME COMMAND
    0 ??  DLs    0:35.41 (swapper)
    1 ??  Is     0:00.26 /sbin/init
    2 ??  DL     0:39.89 (pagedaemon)
  115 ??  Ss     4:27.31 syslogd
  198 ??  Is    55:36.88 update 30
  200 ??  Ss     2:06.16 cron
 7301 ??  Ss     0:12.36 /usr/pkg/sbin/updatesrv -p /vice/db
 7601 ??  Is     0:04.51 auth2
 9624 ??  Ss     0:05.78 updateclnt -h falla -q coda_udpsrv
 9907 ??  S<s    0:11.37 codasrv -t 1000 -trunc 5 -rvm /dev/rwd1a /dev/rwd0g 94
 9899 p0  IW     0:00.02 /bin/sh /usr/pkg/sbin/startserver
```

## Data Location

The information stored on Coda servers is organized into several directories.
These directories are are described below.

| Directory | Description |
| --------- | ----------- |
| `/usr/sbin` | contains the Coda file system binaries for servers and the SCM. |
| `/vice/auth2` | This directory contains information related to the authentication process, including its log file. |
| `/vice/db` | contains the log file for the update processes as well as a number of databases important to servers. |
| `/vice/srv` | contains information related to the server process, including its log file. |
| `/vice/vol` | contains information related to the volumes contained in the Coda file system. |
| `/vice/vol/remote` | exists only on the SCM and contains volume information for all remote servers. |
| `/vice/misc` | is where the updateclnt and updatesrv processes live and store their log files. |

A more detailed description of the files in the these directories can be found in
[System Configuration Files](file_list.md).
