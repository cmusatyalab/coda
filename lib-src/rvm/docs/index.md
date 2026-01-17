---
title: Preface
---
# RVM: Recoverable Virtual Memory, Release 1.3

!!! abstract

    RVM provides an unstructured recoverable virtual memory. The
    recoverable storage is represented by Unix files or disk partitions
    that applications can map at page granularity into the address space
    of a process. Simple, non-nested atomic transactions guarantee
    permanence of changes to recoverable storage across system crashes.
    Applications can schedule transaction logging actions to enhance
    performance. The design stresses simplicity, ease of use, and high
    performance. Unix compatibility is standard, while optional
    Mach-specific extensions are supported for additional flexibility and
    performance. RVM has been extensively used in the clients and servers
    of the Coda File System, and in the Venari system.

## Introduction

RVM, a *Recoverable Virtual Memory* system, is a mechanism to
support persistent virtual memory in the face of system crashes.
The current state of affairs is that software rather than hardware is
the limiting factor in systems reliability[@gray86], and so RVM is
intended to assist in building more robust software.
RVMs principal reliability criterion is to provide permanence of
committed changes and integrity of data over operating system crashes.
The fatal nature of these all-too-frequent events often leaves data
structures in a shambles.

But the all-or-none property of atomic transactions can insure consistency of
data structures over crashes and makes the application significantly more
robust with small cost. Using simple, non-nested local transactions, it
provides a way to automatically checkpoint the state of memory before an
operation and restore that state in the event of a crash.

RVM can also provide some resilience in the face of localized media failures,
but massive media failures such as head crashes are not recoverable since
storage is not mirrored. Applications must rely on periodic backups, or other
mechanisms, to recover from these rare events. While mirroring isn't supported,
RVM does not preclude it: applications can provide device drivers for
storage media that mirror data without RVMs knowledge.

Because RVM is not a server, but a tool for building servers, there are
important differences between RVM and database transaction systems.
RVM does not provide application level synchronization or serialization
functions, although RVM synchronizes its internal actions so that applications
can use multiple threads. Also, RVM manages storage as an unstructured address
space; no object abstractions are supported.

None of these responsibilities are necessary for simple transactions,
although some, or all, will be needed for a given application.
RVMs design is restricted to only the minimum functionality necessary to
support the restoration mechanisms needed for exception handling in a
persistent virtual memory. Other services can be provided by libraries and
servers that use RVM, or by application-specific functions.

Much of the motivation for building RVM has come from experience with
Camelot[@eppinger91], a large, full-featured transaction system.
While Camelot is much more capable than RVM, it was discovered in
building Coda[@satya90y], a high-availability file system, that most
of the benefit of transactions was gained by using only a fraction of
Camelots facilities. RVM is an experiment to discover the minimum
transaction processing functionality useful in building complex
applications such as file servers and other non-database systems.

## Design Assumptions

The decision to use RVM rather than a more complex transaction system can be
made by considering the underlying design assumptions.
If the following constraints are not too restrictive for the application,
RVM will be a good choice:

- Virtual memory can hold all of the applications temporary data.
- The backing store for virtual memory is orthogonal to data storage.
- Robustness in the face of system crashes is the principal reliability
  criterion.
- Transactions do not extend across shutdown or system crashes.
  Transactions must be completed before shutdown and are aborted by a crash.
- Nested transactions are not required.
- A single concurrent server at a single site is sufficient.
- Success of disk operations are correctly reported, and disk sector writes
  are assumed to be atomic: all or none of a sector is written.

These assumptions greatly simplify a transaction system and reflect
RVMs design and implementation philosophy that stresses simplicity and ease
of use. To make RVM highly portable, the implementation uses only Unix features
that are standard across a wide variety of Unix and Mach/Unix implementations.
Mach-specific extensions are used only for optional features and performance
enhancements. Optional features, when provided, exact no penalty when not
chosen; at worst, one will not receive the benefits of performance enhancing
options.

While RVM directly supports only a single server at a single site,
an application can provide the communication mechanisms to integrate multiple
instantiations of RVM into a widely distributed system. RVMs first application,
Coda, will use this technique to manage multiple file servers.

RVM is inoffensive and inconspicuous in action: it does not complicate or
preclude the use of standard tools such as profilers and debuggers.
Recognizing that the disk operations inherent in transaction processing are
expensive, RVM allows the application considerable freedom to manage such
operations.

## Authors

- Henry M. Mashburn
- Mahadev Satyanarayanan
- David Steere
- Yui W. Lee

!!! thanks

    This research was supported by the Defense Advanced Research Projects
    Agency (Avionics Lab, Wright Research and Development Center,
    Aeronautical Systems Division (AFSC), U.S. Air Force, Wright-Patterson
    AFB, Ohio, 45433-6543 under Contract F33615-90-C-1465, ARPA Order
    No. 7597). The views and conclusions in this document are those of
    the authors and do not represent the official policies of the funding
    agency or Carnegie Mellon University.

<!-- date 17 September 1997 -->
