---
title: Preface
---
# RPC2 User Guide and Reference Manual

!!! abstract

    This manual describes the programming interface and internal design of
    *RPC2*, a highly portable and extensible remote procedure call package for
    Unix. RPC2 runs on top of the IP/UDP protocol, and provides support for
    streaming file transfer, parallel RPC calls, and IP-level multicast.  The
    manual also describes *RP2Gen*, a stub generator.

    All the software described in this manual runs at user-level on the Unix
    4.3BSD interface; no kernel modifications are necessary.  The software has
    been ported to a variety of machine architectures (such as IBM-RT, MIPS,
    Sun2, Sun3, Sparc, and i386) and to variants of the Unix operating system
    (such as Mach, SunOS and AIX).

## Preface

RPC2 is a portable and extensible remote procedure call package built on top of
the IP/UDP network protocol.  Since its original use in the Andrew File
System[@howard88], it has been used by the Coda File System[@satya89b], the
Scylla database server and a variety of other applications at Carnegie Mellon
University and elsewhere.  RPC2 runs on LWP, a lightweight process package that
allows multiple non-preemptive threads of control to coexist within one Unix
process.  RP2Gen is a stub generator that simplifies the use of RPC2.  RPC2 and
LWP run entirely at user-level on the Unix 4.3BSD interface; no kernel changes
are necessary.

The first versions of LWP and RPC2 were operational in early 1984 and mid-1985
respectively.  The design of LWP predates that of the Cthreads package in
Mach[@cooper88].  LWP and Cthreads are conceptually similar, but differ
substantially in the details.  It should, in principle, be possible to modify
RPC2 to run directly on top of Cthreads, but we have not tried to do this so
far.  We have successfully emulated all of LWP on top of the preemptive and
non-preemptive versions of Cthreads, and a subset of the non-preemptive version
of Cthreads on top of LWP.

Both LWP and RPC2 have evolved over time, resulting in increased functionality
and robustness.  They have also been ported to a wide variety of machine
architectures, such as IBM-RTs, MIPS, Sun2, Sun3, Sparc, and i386, as well as
variants of the Unix operating systems such as Mach, SunOS and AIX.  The RPC2
extensions include the SFTP side effect mechanism for streaming file transfer,
the MultiRPC mechanism for parallel RPC calls, and IP multicast versions of
MultiRPC and SFTP.  Whenever there has been choice between portability and
machine-specific performance, we have always favored portability.

Although this manual may seem formidable, RPC2 is not difficult to use.  For
simple applications you do not have to know everything described in these
pages.  Advanced features such as use of side effects, MultiRPC, and use of
multicast for parallelism can be ignored initially.

Some day there will be a true tutorial in this manual.  Until then the best way
to learn RPC2 is as follows:

1. Study the overview and examples in [Intro](rpc2_user.md). The latter are
   actual pieces of working code, and you should try running the examples.
   [UsageNotes](usagenotes.md) gives you details of library and include file
   names, current limitations, and other similar details. Read
   [RP2Gen](rp2gen_user.md) next.  This describes the procedural abstraction
   provided by RP2Gen, the stub generator.
2. Read about the [Runtime System](rpc2_runtime.md) and the
   [LWP manual](http://coda.cs.cmu.edu/docs/lwp/), which describe the RPC2 and
   LWP runtime calls, respectively.  You may need to refer to
   [HeaderFiles](https://github.com/cmusatyalab/coda/tree/master/lib-src/rpc2/include/rpc2),
   which contains the header files used by these packages.
3. After you have mastered the basics, you may wish to explore the advanced
   features described in [SFTP](sftp_user.md), [MultiRPC](multi_user.md), and
   [Multicast](multicast.md).
4. If you wish to learn about the internals of RPC2, you may wish to consult
   [Internals](rpc2_internals.md).

## Acknowledgements

The original design, implementation and documentation of RPC2 was by M.
Satyanarayanan.  The MultiRPC implementation and chapter were done by Ellen
Siegel.  The extensions to use multicast were done by James Kistler.  Jonathan
Rosenberg contributed to the RP2Gen implementation and chapter.  Comments from
Robert Sidebotham, David Nichols, Vasilis Apostolides, Drew Perkins, Bradley
White, Stephen Hecht and many other users have been helpful in the improvement
of RPC2.

<!--
The original design and implementation of LWP was done by Larry Raper.  Its
documentation descends from a manual by Jonathan Rosenberg and Larry Raper,
later extended by David Nichols and M. Satyanarayanan.
-->
Richard Draves, Qi Lu, Anders Klemets and Gowthami Rajendran helped in revising
and improving this document.

## Authors

- M. Satyanarayanan (Editor)
- Richard Draves
- James Kistler
- Anders Klemets
- Qi Lu
- Lily Mummert
- David Nichols
- Larry Raper
- Gowthami Rajendran
- Jonathan Rosenberg
- Ellen Siegel

!!! Thanks

    This work has been supported by the Defense Advanced Research Projects
    Agency (Avionics Lab, Wright Research and Development Center, Aeronautical
    Systems Division (AFSC), U.S. Air Force, Wright-Patterson AFB, Ohio,
    45433-6543 under Contract F33615-90-C-1465, ARPA Order No. 7597), the
    National Science Foundation (CCR-8657907), and the IBM Corporation (Andrew
    Project, Faculty Development Grant, Research Initiation Grant), and
    Digital Equipment Corporation (External Research Project).  The views and
    conclusion expressed in this paper are those of the authors, and should
    not be interpreted as those of DARPA, NSF, IBM, DEC or Carnegie Mellon
    University.
