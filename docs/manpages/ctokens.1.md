---
title: CTOKENS(1)
footer: Coda Distributed File System
date: 2005-04-25
---

## NAME

ctokens - List Coda tokens

## SYNOPSIS

**ctokens** \[&commat;realm]

## DESCRIPTION

The **ctokens** command enables users to query **venus** for information
about the set of authentication tokens it holds for the user. The
**ctokens** command prints out the ID of the current user and whether or
not the user is currently authenticated.

You can optionally specify Coda realm to query the token availability
for that specific realm.

## SEE ALSO

**clog**(1), **cunlog**(1)

## AUTHORS

- 1987, Adapted from AFS-2s unlog
- David C.\ Steere, 1990, Created man page
