---
title: AU(1)
footer: Coda Distributed File System
date: 2005-04-25
---

## NAME

au - Activate user account

## SYNOPSIS

**au** \[-x] \[-h host] \[-p port] **nu** | **cp** | **cu** | **du**

## DESCRIPTION

**au** is used by an administrator to add, delete, or change user information.
**au** will first prompt for the administrators Coda user name and password. It
will then connect to the **auth2** daemon on the server to perform the
requested operation.

**au** supports the following options:

**-x**

:   Turns on debugging.

**-h**

:   Attach to *host* to do the authentication. *host* should be the SCM
    server.

**-p**

:   *port* to bind to. The default port is codaauth2 (370/udp).

**nu**

:   New user. The **nu** option tells the **auth2** daemon to add a new
    password entry to the Coda password database. Note that the user has
    to be known to coda in beforehand. This is done via **pdbtool** *new user*.

**cp**

:   Change password. Use this to change a users vice password.

**cu**

:   Change user information. Use this to change password and other user
    information.

**du**

:   Delete user. The **du** option tells the **auth2** daemon to remove
    the password from the Coda password database, thereby disabling the
    account.

## DIAGNOSTICS

You must be a Coda system administrator to run **au**.

## BUGS

**au** echos new passwords to the terminal as they are typed in.

## SEE ALSO

**pdbtool**(8)

## AUTHORS

- 1987, Adapted from AFS-2
- Joshua Raiff, 1993, Created man page
