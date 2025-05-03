---
title: UPDATECLNT(8)
footer: Coda Distributed File System
---

## NAME

updateclnt - update client executables

## SYNOPSIS

**updateclnt** \[-d debug\_level] \[-h host\_name] \[-q service\_name]
\[-w wait\_interval] \[-r reps]

## DESCRIPTION

The **updateclnt** command is a client process to the **updatesrv**
process. It is used to keep the binaries and data bases on a file server
in sync with those on a control machine. The arguments include:

**-d** *debug_level*

:   This option changes the debugging level. The higher the debug\_level,
    the more information is printed. Maximum debugging level is 10.

**-h** *host_name*

:   This is the hostname of the control machine with which this process
    keeps up-to-date. Default: scm.

**-q** *service_name*

:   This is the name in `/etc/services` that should be used as the connect
    portal on the machine specified by **host\_name** above. Default: rupdsrv.

**-w** *wait_interval*

:   The interval between probes, in seconds. The lower this number, the
    quicker the servers will be updated when a change occurs and the more
    cpu and network resources required. Default: 300s (5min).

**-b**

:   This option forces a .BAK file to be kept for any file that is changed.

**-r** *reps*

:   This is the number of wait intervals between long checks. Files are
    checked at two intervals. The interval specified by **-w** is used for
    those files in `/vice/db`. All other files are checked once each **-r**
    repetitions of length **-w**. Default: 6, so the long interval is 30min.

**Updateclnt** checks `/vice/db` every *wait\_interval* seconds. This
directory contains a file called `files`. Each file specified in it has
its current date checked at the server, and a new copy of the file is
fetched if the date does not match that on the control machine. The
format of the `files` file is defined below.

## EXAMPLES

The command `updateclnt -h mahler` would cause the client machine to
check the host "mahler" every 5 minutes to see if any of the
`/vice/db` files have changed and every 30 minutes to see if any other
files have changed. Normally the command is invoked by issuing the
**updatemon**(8) command with the same operands.

## FORMAT

The format of the `files` file is one file name per line. A "-" as the
first character on the line causes the file to be deleted instead of
copied.

## DIAGNOSTICS

The updateclnt program can have its debug level turned on while it is
running by sending a `kill -TSTP` signal to a running updateclnt. To
reset the debug level back to zero, send a `kill -HUP` signal to the
running updateclnt. This also causes the UpdateLog file to not be
written anymore.

## FILES

- `/vice/misc/UpdateClntLog` - is the name of the log file for the updateclnt command.
- `/vice/db/update.tk` - is the shared secret used to authenticate with **updatesrv**.

## BUGS

There is no easy way to add to the list of directories checked.

The **-r** option is now obsolete as **updateclnt** does not update any
directory other then `/vice/db`.

## SEE ALSO

**updatesrv**(8)

## AUTHOR

- Maria R.\ Ebling, 1990, Adapted from AFS
