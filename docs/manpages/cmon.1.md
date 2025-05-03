---
title: CMON(1)
footer: Coda Distributed File System
date: 2005-04-25
---

## NAME

cmon - Coda server monitor

## SYNOPSIS

**cmon** \[-a] \[-t probeinterval] server \[server ...]

## DESCRIPTION

**cmon** probes the specified list of servers once every
**probeinterval** seconds and reports on their status. If a server is
down or unreachable, **cmon** tries to reestablish contact with it once
every **probeinterval** seconds. It uses the **curses**(3) package for
screen management and can be used on dumb terminals. Run **cmon** in a
terminal emulator (like **xterm**(1)).

Some **cmon** data can be displayed in relative or absolute modes. In
relative mode, data is reported with reference to the interval between
the last two probes. In absolute mode, the accumulated value since
initialization is reported. **cmon** can be toggled between absolute and
real modes of presentation from the keyboard. Typing a will cause data
to be presented in absolute mode. Typing r will present it in relative
mode. A mode change will only take place at the next probe.

## OPTIONS

The command-line options are:

**-a**

:   Report data in absolute mode.

**-t**

:   Probe servers every *probeinterval* seconds. Default is 60.

## OUTPUT

The data reported by **cmon** is organized under four headings: **TIM**,
**CPU**, **RPC**, and **DSK**.

The **TIM** data is as follows:

**mon**

:   time at which this **cmon** process was created.

**prob**

:   time at which the server was last probed.

**succ**

:   time at which the server last responded to a probe.

**up**

:   time at which the server process was started.

**bind**

:   number of times contact was reestablished after a failed probe. A
    probe may fail due to server or network failures.

The **CPU** data is as follows:

**sys**

:   Relative or absolute number of seconds of system CPU time used on
    the server.

**user**

:   Relative or absolute number of seconds of user CPU time (regular or
    niced) used on the server.

**util**

:   Relative or absolute number of system and user seconds divided by
    corresponding time of accumulation.

The **RPC** data is as follows:

**conn**

:   Number of RPC connections.

**wkst**

:   Number of workstations connected to server. Note that each instance
    of **cmon** shows up as a workstation.

**call**

:   Relative or absolute number of RPC calls received.

**pki**

:   Relative or absolute number of RPC packets received. Includes
    duplicates and other bogus packets. Also includes bulk transfer
    packets.

**pko**

:   Relative or absolute number of packets sent. Includes
    retransmissions. Also includes bulk transfer packets.

**byi**

:   Bytes corresponding to **pki**.

**byo**

:   bytes corresponding to **pko**.

The **DSK** data is as follows:

**max1**

:   Identity and percent usage of most full disk partition on server.
    The identity is the name of mount point. Names longer than 5
    characters are truncated to the first 3 characters, a \$ character,
    and the last character.

**max2**

:   Identity and percent usage of second most full disk.

**max3**

:   Identity and percent usage of third most full disk.

## DIAGNOSTICS

If a server is down or unreachable, statistics for it are reported as
"???".

Relative data is undefined until two or more probes have been made. Such
data is reported as "\*\*\*" between the very first and second probes.

## BUGS

- Relative computations are just based on the difference between the
  two most recent probes. A smarter approach (especially for CPU
  utilization) would be to use some kind of weighted history.
- The display is optimized for maximum packing of information into a
  small screen area. It may be cryptic for a novice.
- No disk information is available about the root partition on a server.
- Will core dump if run in a window with fewer than 25 lines.
- No way to force a redisplay (eg \^L).

## SEE ALSO

**codacon**(1), **vutil**(8), **curses**(3)

## AUTHORS

- M.\ Satyanarayanan, 1990, Created
