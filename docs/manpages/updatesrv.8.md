---
title: UPDATESRV(8)
footer: Coda Distributed File System
---

## NAME

updatesrv - update server files

## SYNOPSIS

**updatesrv** \[-d debug\_level] \[-l number\_of\_lwps] \[-p prefix] \[-s]
\[-port listen\_port]

## DESCRIPTION

The **updatesrv** command is a read-only server that responds to
requests for files by name. The operands that it accepts are:

**-d** *debug_level*

:   This is used to increase the level of messages written to the log file.

**-l** *number_of_lwps*

:   This operand is used to specify how many lwps will be used to answer
    file requests. Default: 2.

**-p** *prefix*

:   This name, prefixed to all file requests, is used to limit the files
    that can be requested.

**-port** *listen_port*

:   This is the UDP port on which we should be listening. If not specified
    updatesrv will use a random port.

## EXAMPLES

The command `updatesrv -s -p /` would invoke the updatesrv process and
cause the files to be served relative to `/` on the server machine.

The command `updatesrv -s -p /vice/bin` would cause the files to be served
relative to `/vice/bin` on the server machine.

## DIAGNOSTICS

The UpdateLog file will contain useful information. Issuing a `kill -TSTP`
signal to a running updatesrv will increase the level of debugging
messages. The debugging messages can be turned off by issuing a `kill -HUP`
signal. This will also cause the log file to no longer be written.

## FILES

- `/vice/misc/UpdateSrvLog` - Log of the messages from **updatesrv**.
- `/vice/db/update.tk` - is the shared secret used to authenticate with **updatesrv**.

## SEE ALSO

**checkvenus**(8), **updateclnt**(8)

## AUTHOR

- Maria R.\ Ebling, 1990, Adapted from AFS-2
