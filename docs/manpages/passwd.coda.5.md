---
title: PASSWD.CODA(5)
footer: Coda Distributed File System
date: 2005-04-25
---

## NAME

passwd.coda - Coda user identification file

## DESCRIPTION

This file specifies initial cleartext passwords for Coda users. The
format of the file is:

    uid<TAB>actual cleartext password<TAB>any desired info

Where:

*uid*

:   is a Coda user id.

*Cleartext Password*

:   is the password to use for this *uid*.

*Other user info*

:   is the other information about the user you want to include, e.g.
    the user name.

**WARNING** this file must be owned by root and not be readable by any
other user.

This file is translated into a Coda encrypted password file by **initpw**(8).

## SEE ALSO

**initpw**(8), **pdbtool**(8)

## AUTHORS

- Lesa Gresh, 1998, created
