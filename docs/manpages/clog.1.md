---
title: CLOG(1)
footer: Coda Distributed File System
date: 2005-04-25
---

## NAME

clog - Authenticate to the Coda distributed file system

## SYNOPSIS

**clog** \[-pipe] \[-test] \[-host hostname] \[-tofile file] \[-fromfile file] \[username]\[&commat;realm]

## DESCRIPTION

This command enables users to get tokens to use for authenticated
communication with a Coda File System. The given name and a password are
passed to an **Authentication** Server. The **Authentication Server**
returns a set of tokens encoding the chosen identity if the password is
correct. **clog** passes these tokens to the **venus** process, which
acts as the users agent for file manipulation. These tokens have a
limited lifetime, becoming invalid after 25 hours. If an attempt is made
to operate on a Coda File System object while the user is not
authenticated, the user will assume the privileges of the **Anonymous**
user.

**clog** accepts the following arguments:

**-pipe**

:   For use in shell scripts, the password is read from stdin. This
    option is also enabled when stdin is not a tty.

**-test**

:   Test the clog-venus token passing code.

**-host**

:   Specify the host where the authentication server is running.

**-tofile**

:   After obtaining a token from the authentication server, write it to
    a file.

**-fromfile**

:   Instead of connecting to the authentication server, read a
    previously saved token from a file and pass it to venus.

**\[username]\[&commat;realm]**

:   Coda user name you wish to be identified as within the specified
    realm. If the username is not specified, clog tries to use the local
    login name. If the Coda realm name is not specified clog falls back
    to using the default realm name as specified in the venus.conf
    configuration file.

## BUGS

- The on-disk tokens are not encrypted.
- The **-pipe** option should be dropped, there already is an isatty(stdin)
  test which is sufficient for most cases.
- The **-test** option should also test the reading and writing code of token
  files.

## SEE ALSO

**cunlog**(1), **venus**(8)

## AUTHORS

- 1987, Adapted from AFS-2
- Maria R.\ Ebling, 1990, Created man page
