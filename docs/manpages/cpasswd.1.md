---
title: CPASSWD(1)
footer: Coda Distributed File System
date: 2005-04-25
---

## NAME

cpasswd - Change Coda login password

## SYNOPSIS

**cpasswd** \[-h host] \[username]\[&commat;realm]

## DESCRIPTION

This command sets the Coda password for the current user.

**cpasswd** prompts for the users old password, and then for the new
one. The new password must be typed twice, to forestall mistakes. Change
requests can be aborted by typing a carriage return when prompted for
the new password.

**cpasswd** won't be able to change the users password if the password
server is down. An appropriate error message will be printed, advising
the caller to try again later. Once the password server acknowledges
receipt the users new password, it should not take too link for the
information to propagate to the entire set of **Authentication
Servers**.

**cpasswd** supports the following option:

**-h**

:   Tells **cpasswd** which *host* to bind to. This should be the SCM.

**\[username]\[&commat;realm]**

:   Coda user name and the realm of the user whose password you want to
    change. If the username is not specified, clog tries to use the
    local login name. If the Coda realm name is not specified clog falls
    back to using the default realm name as specified in the venus.conf
    configuration file.

## SEE ALSO

**clog**(1)

## AUTHOR

- 1987, Adapted from AFS-2
- Maria R.\ Ebling, 1990, Created man page
