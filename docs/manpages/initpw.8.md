---
title: INITPW(8)
footer: Coda Distributed File System
date: 2005-04-25
---

## NAME

initpw - Initialize the auth2 password database

## SYNOPSIS

**initpw** \[-k key]

## DESCRIPTION

**initpw** initializes the **auth2** password database by reading
entries as specified in **passwd.coda**(5) and writes the corresponding
encrypted entries to stdout.

**initpw** supports the following command line options:

**-k**

:   Using the **-k** option allows you to specify the encryption key to
    use when encrypting cleartext passwords. Currently you must give
    "drseuss " as the key.

## EXAMPLE

    initpw -k "drseuss " < /vice/db/passwd.coda > /vice/db/auth2.pw

## SEE ALSO

**auth2**(8), **passwd.coda**(5)

## AUTHORS

- 1987, Adapted from AFS-2
- Joshua Raiff, 1993, Created man page
- Lesa Gresh, 1998, modified
