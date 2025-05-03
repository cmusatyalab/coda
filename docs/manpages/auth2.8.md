---
title: AUTH2(8)
footer: Coda Distributed File System
date: 2005-04-25
---

## NAME

auth2 - Coda authentication server

## SYNOPSIS

**auth2** \[-r] \[-chk] \[-x debuglevel] \[-p passwordfile] \[-tk tokenkey]
\[-fk filekey]

## DESCRIPTION

**auth2** is the Coda authentication server. Clients need to have an
authentication token from **auth2** in order to establish authenticated
connections to Coda file servers.

**auth2** supports the following commands line options:

**-r**

:   Tells **auth2** to print log entries to the controlling tty rather
    than a log file.

**-chk**

:   Causes **auth2** to only validate passwords. New password entries
    cannot be added via this server instance. Servers started with the
    **-chk** option are slave servers. There should only be one master
    server (**SCM**) in the system.

**-x**

:   Sets the server *debuglevel*. This level controls the amount of
    debugging information printed by the server.

**-p**

:   Used to tell **auth2** where its password file is. Typically this
    file is `/vice/db/auth2.pw`.

**-tk**

:   Specifies where the *tokenkey* file is found which **auth2** should
    use when encrypting tokens. Normally this file is `/vice/db/auth2.tk`.

**-fk**

:   Name of file containing the key to decrypt the password file with.

## AUTHORS

- 1987, Adapted from AFS-2
- Joshua Raiff, 1993, Created man page
