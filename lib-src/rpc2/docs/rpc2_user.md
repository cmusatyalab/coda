# RPC2 Overview and Examples

_Remote Procedure Call (RPC)_ is a paradigm of communication between a client
and a server.  Interactions consist of an alternating  sequence of brief
requests by the client, and brief replies by the server.  Each request consists
of an _Operation_ and a set of _Parameters_.  A reply consists of a _Return
Code_ and a set of result  parameters.  Site and communication failures are
synchronously detected.

## Components

RPC2 consists of two relatively independent components: a Unix-based run time
library written in C, and a stub generator RP2Gen.  The run-time system is self
contained and usable in the absence of RP2Gen.  The RPC2 interface, which
describes the characteristics of the procedures provided by the server to the
clients is defined in a file with extension `.rpc2` or `.rpc`.  RP2Gen takes a
description of the interface and automatically produces the client and server
stubs, and the code for marshalling.

The RPC2 library is used in conjunction with the _Lightweight Process (LWP)_
library.  The client and server code together with the respective stubs and the
RPC2 and LWP libraries are all linked together to generate the client and
server object binaries.  Although RPC2 and LWP are written in C, they have been
successfully used by programs used in C++.

## Client-Server Model and Binding

The concepts of _server_ and _client_ are quite general.  A server may be the
client of one or more other servers.  A server may have one more _subsystems_
associated with it, each subsystem corresponding to a set of
functionally-related procedure calls.  Binding by clients is done to a
host-port-subsystem triple.  There is no a priori restriction (other than
resource limitations) on the number of clients a server may have, or on the
number of servers a client may be connected to.  Host, port, and subsystem
specifications are discriminated union types, to allow a multiplicity of
representations.  Thus hosts may be specified either by name or by Internet
address.

## How RPC2 uses LWP

Clients and servers are each assumed to be Unix processes using the
LWP library. RPC2 will not work independently of the LWP library.  The LWP
library makes it possible for a single Unix process to contain multiple threads
of control (LWPs). An RPC call is synchronous with respect to an individual
LWP, but it does not block the encapsulating Unix process.  Although LWPs are
non-preemptive, RPC2 internally yields control when an LWP is blocked awaiting
a request or reply.  Thus more than one LWP can be concurrently making RPC
requests. There is no a priori binding of RPC connections to LWPs, or LWPs to
subsystems within a client or server. Thus RPC connections, LWPs and subsystems
are completely orthogonal concepts.

Since LWPs are non-preemptive, a long-running computation by an LWP will
prevent the RPC2 code from getting a chance to run.  This will typically
manifest itself as a timeout by the client on the RPC in progress.  To avoid
this, the long running computation should periodically invoke `IOMGR_Poll`
followed by `LWP_DispatchProcess`.

For similar reasons, Unix system calls such as `read(2)`, `sleep(3)`, and
`select(2)` that may block for a long time should be avoided. Instead, use
`IOMGR_Select`.

## Side Effects

RPC connections may be associated with _Side-Effects_ to allow
application-specific network optimizations to be performed.  An example is the
use of a specialized protocol for bulk transfer of large files.  Detailed
information pertinent to each type of side effect is specified in a _Side
Effect Descriptor_.

Side effects are explicitly initiated by the server using `RPC2_InitSideEffect`
and occur asynchronously.  Synchronization occurs on an `RPC2_CheckSideEffect`
call by the server.  Each client-server connection deals with one kind of side
effect; however different connections may use different kinds of side effects.

Adding support for a new type of side effect is analogous to adding a new
device driver in Unix.  To allow this extensibility, the RPC code has hooks at
various points where side-effect routines will be called.  Global tables
contain pointers to  these side effect routines.  The basic RPC code itself
knows nothing about these side-effect routines.

## Security

A _Security Level_ is associated with each connection between a client and a
server.  Currently four levels of security are supported:

- neither authenticated, nor secure
- authenticated, but not secure
- authenticated, and headers secure
- authenticated, and fully secure

_Authenticated_ in this context means that the client and server start out as
mutually suspicious parties and exchange credentials during the establishment
of a connection.  A secret encryption key, known a priori only to the server
and client is used in authentication handshakes.  The secret key itself is
never sent over the network.  The handshake is a variant of the well-known
Needham and Schroeder protocol[@needham78].

_Secure_ means that transmitted data is immune to eavesdropping and undetected
corruption.  This is achieved by encryption, using a session key generated
during connection establishment.  No attempt is made, however, to guard against
traffic analysis attacks.

The RPC library makes no assumptions about the format of client identities or
about the mapping between clients, servers and shared secret keys.  A
server-supplied procedure is invoked during the authentication sequence to
validate client identities and obtain keys.  The paper on authentication in the
Andrew File System[@satya89c] gives a detailed example of how this
functionality may be used.

## Examples

In  this section we walk the reader through examples of increasing complexity,
thereby demonstrating the use of RPC2.  There are two examples: a relatively
simple one using a single LWP and exporting a single subsystem, followed by a
more complex one using multiple LWPs and subsystems.  There is a Makefile for
both these examples here [Makefile](examples/makefile.md).  Neither example
uses side effects.  An example using side effects can be found in the [SFTP
Userguide](sftp_user.md).

### Example 1: Getting Time of Day

This is a very simple example that has only one subsystem telling the client
the time of the day in seconds and micro seconds on the server machine.

The interface is defined in the `rtime.rpc` file.  This file is used as input
to RP2Gen to generate the client and server stubs `rtime.client.c` and
`rtime.server.c`

The first step at both the client and the server is initialization of the LWP
and RPC2 libraries.  This is done by calling the `LWP_Init` and `RPC2_Init`
calls respectively.  It is necessary that the LWP library be initialized before
the RPC2 library.

The client, after initialization, binds to the server by calling
`RPC2_NewBinding`.  The parameters to this call include the identity of the
server, the subsytem etc.  A connection id is returned to the client.  The
client is now ready to make RPC calls on this connection.

The server indicates its willingness to accept calls for the `rtime` subsytem
by calling `RPC2_Export`.  A filter for the `rtime` subsytem is specified
and used in awaiting an RPC request by calling `RPC2_GetRequest`.  On receiving
a request,  the server decodes and executes it by calling
`rtime_ExecuteRequest`.  `rtime_ExecuteRequest`, which is actually defined in
the server stub file, calls the server code for the `GetRTime` routine and
sends the response to the client.  The server then goes back to waiting for
more requests.

#### Interface Specification

:   [`rtime.rpc`](examples/rtime.rpc.md)

#### RTime Client Code

:   [`rtime_clnt.c`](examples/rtime_clnt.c.md)

#### RTime Server Code

:   [`rtime_srv.c`](examples/rtime_srv.c.md)

### Example 2: Authentication and Computation Subsystems

In this example there are two subsystems: an authentication subsystem
and a computation subsystem.  Each subsystem services a different
set of calls.

In this example, two LWP processes are created at the server,
one for each subsytem to be serviced.  But this is not essential.
An alternative would have been for a single LWP to service calls
for both subsystems or for a collection of LWPs to service calls
for any subsystems.

#### Interface Specifications for Auth Subsystem

:   [`auth.rpc`](examples/auth.rpc.md)

#### Interface Specifications for Comp Subsystem

:   [`comp.rpc`](examples/comp.rpc.md)

#### AuthComp Client Code

:   [`authcomp_clnt.c`](examples/authcomp_clnt.c.md)

#### AuthComp Server Code

:   [`authcomp_srv.c`](examples/authcomp_srv.c.md)

### Makefile for All Examples

:   [`Makefile`](examples/makefile.md)

In [RP2Gen Stub Generator](rp2gen_user.md) we will see how the stub generator
can create such files.
