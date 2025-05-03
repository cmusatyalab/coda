---
title: STARTSERVER(8)
footer: Coda Distributed File System
---

## NAME

startserver - Start the CFS file server

## SYNOPSIS

**startserver**

## DESCRIPTION

**startserver** is used to start the Coda file server process. It will
clean up old log files, then start the server. This script is useful for
starting the server with the same configuration every time its run.

## FILES

- `codasrv`
- `/etc/coda/server.conf`
- `/vice/srv/SrvLog*`
- `/vice/srv/SrvErr`

## SEE ALSO

**codasrv**(8)

## AUTHOR

- Joshua Raiff, 1993, Created man page
