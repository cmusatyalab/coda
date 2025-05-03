---
title: PDBTOOL(8)
footer: Coda Distributed File System
date: 2005-04-25
---

## NAME

pdbtool - Manage user/group database on Coda server

## SYNOPSIS

**pdbtool** \[command]

## DESCRIPTION

This command provides an interface that allows one to manage and control
the user database. This is the key database behind the **Authentication
Server**. The database maintains the CPS for each user, which is given
to venus when a user logs in. The **pdbtool** allow the system
administrator to create new users and groups, plus control which groups
users are members of. **pdbtool** provides a simple user interface and
is usable on the server system only. Type **help** to list the commands.

**pdbtool** accepts any of the commands as an argument. If no arguments
are given it prompts for a command.

## BUGS

Is still somewhat difficult to use and may not provide all of
functionally required by some users. Has hard coded file locations. :)

## SEE ALSO

**au**(1), **auth2**(8)

## AUTHORS

- Mathew G.\ Monroe, 1998-1999, Coda 5.0, Created
