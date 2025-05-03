---
title: SPY(1)
footer: Coda Distributed File System
date: 2005-04-25
---

## NAME

spy - Report all Coda file that are opened

## SYNOPSIS

**spy** \[-host hostname] \[-uid uid]

## DESCRIPTION

**spy** will connect to **venus** on the given host and report all Coda
files that are opened. This is useful for generating **hoard** files as
programs can have hidden dependencies. Using spy will ensure that any
file that is opened gets included in the **hoard** database. Sending
**spy** a **SIGTERM** will cause **spy** to flush all of its output
buffers and terminate.

**spy** supports the following options:

**-host**

:   Name of Coda coda client to bind to. If this option is omitted, then
    the current host will be used.

**-uid**

:   Only report on file activity from user *uid*. If the **-uid** switch
    is omitted, then all CFS file activity is reported.

To use **spy** to create a **hoard** file sort the output and remove any
duplicates, By using **spy** you ensure that you did not forget to
**hoard** a file with a hidden dependency. For example, when you **hoard
X**, you might need the **uncompress**(1) program in order to use the
fonts.

## SEE ALSO

**hoard**(1)

## AUTHORS

- Joshua Raiff, 1993, Created man page
