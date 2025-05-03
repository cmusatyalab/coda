---
title: Preface
---
# Coda File System User and System Administrator Manual

!!! abstract

    The *Coda File System* is a descendant of the *Andrew File System.*
    Like AFS, Coda offers location-transparent access to a shared Unix file
    namespace that is mapped on to a collection of dedicated file servers.  But
    Coda represents a substantial improvement over AFS because it offers
    considerably higher availability in the face of server and network failures.
    The improvement in availability is achieved using the complementary techniques
    of *server replication* and *disconnected operation*.  Disconnected operation
    has proven to be especially valuable in supporting portable computers. This
    document is a reference manual for Coda users and system administrators.

## Preface

Welcome to Coda!  Whether you are at Carnegie Mellon or elsewhere, we hope that
this manual will help you make effective use of the Coda File System.  The
manual is written from two different perspectives: end user and system
administrator. If your organization is already running Coda, and you need to
learn how to use it - [Getting Started](getting_started.md) and [Common
Scenarios](scenarios.md) should fully answer your questions. For more
adventurous among you, who want to maintain your own Coda client, please read
[System Overview](system_overview.md) through
[Client Installation](client_installation.md). The rest of the book is
dedicated to the specifics of system administration.

In writing this manual, we have assumed that you are an experienced Unix user,
familiar with the broad concepts of distributed file systems.  If you are a
system administrator, we assume that you are familiar with administering Unix
systems.  You will find Coda especially easy to use if you are already fluent
in using AFS.

We have also assumed that you are familiar with the design goals and
architecture of Coda.  The best way to learn about these is to read the
overview and design rationale papers on Coda
[[satya90z](bibliography.md#fn:satya90z)]
[[satya90y](bibliography.md#fn:satya90y)]
[[kistler92](bibliography.md#fn:kistler92)].  We urge you to obtain copies of
these papers and to read them before attempting to use Coda.  Those papers
provide context and information upon which this manual relies and does not
repeat.

Since Coda is an experimental system and not a commercial product, you will
inevitably encounter rough edges.  Within the limits of our resources, we will
do our best to fix these problems and to improve the system over time.  But we
do ask that you try to characterize the problem as accurately as possible, and
to try and obtain a repeatable and concise instance of it.

After you have taken the time to learn the system well, please give us your
feedback.  We would like to improve both the system and this manual.

## Further Reading

Besides the three papers mentioned above, there are a number of papers that
address specific aspects of Coda. These include:

1. A description of the MiniCache, which allows the client manager to reside
   outside the kernel without excessive loss of performance
   [[steere90](bibliography.md#fn:steere90)].
1. A description and performance analysis of the technique used in Coda for
   transparent directory resolution [[kumar93](bibliography.md#fn:kumar93)].
1. A manual on the remote procedure call, RPC2, and threading, LWP, mechanisms
   used in Coda [[satya91](bibliography.md#fn:satya91)].
1. A paper and manual on the transactional facility, RVM, used on Coda clients
   and servers [[satya93](bibliography.md#fn:satya93)]
   [[mashburn92](bibliography.md#fn:mashburn92)].
1. A detailed description of support for disconnected operation in Coda
   [[kistler93](bibliography.md#fn:kistler93)].  Chapter 4 in that document
   offers an excellent overview of the implementation structure of clients and
   servers.
1. A discussion of relevant security issues and mechanisms to address them
   [[satya89c](bibliography.md#fn:satya89c)].  This paper discusses security in
   AFS v2, but the mechanisms in Coda are virtually identical.

## Acknowledgments

Coda is the work of many individuals.  Contributors to the design and
implementation of various aspects of the system include: Jay Kistler, Puneet
Kumar, David Steere, Lily Mummert, Maria Ebling, M. Satyanarayanan, Hank
Mashburn, Brian Noble, Lu Qi, Josh Raiff, Ellen Siegel, Anders Klemets, and
Kudo Masahi.  Many of these individuals have also contributed to the writing of
this manual.  The system has improved considerably in response to feedback from
users outside the Coda project.  The earliest of these users (Tom Mitchell,
Manuela Veloso, and Matt Zekauskas) deserve special thanks for their
willingness to sail into uncharted waters!

## Authors

- M. Satyanarayanan
- Maria R. Ebling
- Joshua Raiff
- Peter J. Braam
- Jan Harkes
