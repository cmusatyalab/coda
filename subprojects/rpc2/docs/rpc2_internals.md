# RPC2 Internals

## Background

This chapter describes the internal structure of RPC2.  It describes the most
relevant data structures and routines used in RPC2.  This chapter will not be
of interest to the average user of RPC2.  It is intended for system programmers
who are interested in knowing the internal workings of RPC2.   It is not a
substitute for source code, but is an annotated guide for the RPC2 source code.
As previously mentioned, the RPC2 package is used in conjunction with the LWP
library and its components, the IOMGR package and the TM package.  This
document refers to these packages only in relation to RPC2. For more in-depth
information on LWP see the [LWP manual][lwp-manual].  Discussion of side
effects is described in [SFTP Internals](sftp_internals.md) and [Adding New
Kinds of Side Effects](se_internals.md).

[lwp-manual]: http://coda.cs.cmu.edu/docs/lwp/

RPC2 consists of two relatively independent components: a Unix-based run time
library written in C, and a stub generator RP2Gen.  The RPC2 runtime consists
of the base RPC2 code and a set of routines for each of the side effects
described.  The run-time system is self-contained and usable in the absence of
RP2Gen.  In this chapter we will describe some of the routines that make up the
run time library.

The LWP package supports multiple nonpreemptive threads of control within a
single Unix process.  When a remote procedure is called, the calling LWP is
suspended until the response is received.  While the LWP is suspended, the RPC2
runtime system internally yields control and other LWPs can make concurrent RPC
requests.  Hence multiple RPCs can be in progress simultaneously.  This becomes
particularly important when long running side effects occur.

## The LWP library

### Basic routines

The LWP routines that are most relevant to RPC2 are the following.

- `LWP_Init` - initializes the LWP package, and turns the calling process into
  the initial thread with the specified priority.
- `LWP_CreateProcess` - creates a new LWP thread.  This call causes the
  scheduler to be invoked.
- `LWP_DispatchProcess` - the calling thread yields to the LWP scheduler.
- `LWP_NoYieldSignal` - signals an event but does not yield to the scheduler.

Both the RPC2 and the LWP package are entirely outside the Unix Kernel, and
depend only on the 4.3 BSD interface.  At present RPC2 runs on the DARPA IP/UDP
protocol via Unix sockets.

### IOMGR routines

Routines in the IOMGR package allows LWPs to wait on various Unix events.  When
the package is initialized, an IOMGR thread is created.  This thread runs at
the lowest priority.  In order for the IOMGR package to function correctly, all
other threads must run at a higher priority.

A common mistake in user code is to assume that yielding by a user LWP (via
`LWP_DispatchProcess()` will allow the IOMGR LWP to run; this will be true
**only** if the user LWP runs at the lowest priority.  In all other cases, the
routine `LWP_Poll()` must be called prior to `LWP_DispatchProcess`.  The call
to `LWP_Poll()` executes the same code that would have been executed by the
IOMGR LWP, had it been given a chance to run.  `LWP_DispathProcess()` allows
LWPs waiting on IOMGR events to run, if the `LWP_Poll()` detects such events.

The IOMGR call that is most relevant to the RPC2 package is the

- `IOMGR_Select`.
  `IOMGR_Select` allows a light-weight process to wait on the same set of
  events that the Unix select waits on.  The parameters are the same.
  `IOMGR_Select` puts the caller to sleep until no user processes are active.
  At this time the IOMGR thread, which runs at the lowest priority, wakes up,
  coaleses all the LWPs blocked by a select performs a single `select` and
  wakes up all processes affected by the result.

### TM routines

The Timer package is used to create and maintain timers for different events
associated with RPC2 calls (for instance timeouts).  This package contains a
number of routines that assist in manipulating lists of timers.  The timers are
assigned a timeout value by the user and inserted in a list that is maintained
by the package. There are routines to check for expired timers, to update
timers etc.  The TM package routines that are relevant to RPC2 are the
following.

- `TM_Insert` - initializes a timer element and inserts it into a timer list.
- `TM_Rescan` - updates the timer elements in the specified list and looks for
  expired elements.
- `TM_GetExpired` - returns an expired timer from a list.
- `TM_GetEarliest` - returns the earliest timer on a list.

## The RPC2 package

### Examples

[RPC2 Overview and Examples](rpc2_user.md) contains two examples of RPC2
subsystems.  We consider the example in [Example 1: Getting Time of
Day](rpc2_user.md#example-1-getting-time-of-day).  For this example, RP2Gen
generates the server and client stubs shown below.

#### Client Stub for Example 1

:   [`rtime_clnt.c`](examples/rtime_clnt.c.md)

#### Server Stub for Example 1

:   [`rtime_srv.c`](examples/rtime_srv.c.md)

### Initialization and Thread Creation

RPC2 provides logical connections for the client to communicate with the
server.  The client and server communicate via internet sockets.  Both at the
client and at the server end, the sockets are monitored by a LWP called the
SocketListener thread.

The SocketListener thread which is created during initialization is an integral
part of the RPC2 package.  In addition to monitoring the socket, it receives
and decodes the packet and then notifies the appropriate LWP which is awaiting
a packet.  Given that multiple LWPs can be waiting for packets, the job of the
SocketListener thread is to demultiplex incoming packets among multiple waiting
LWPs.

Besides the SocketListener thread, there is an IOMGR thread.  Both these
threads are created during initialization of the RPC2 package, via the
`RPC2_Init` primitive.  Besides these internal threads, clients and servers may
have any number of other threads limited only by the amount of memory.  A
server may be organized in many ways: one thread per subsystem, a pool of
threads servicing any of a number of subsystems, and so on.

The interplay between threads is as follows.  The client makes an RPC and goes
to sleep, thereby handing control to the SocketListener thread.  This thread
then blocks on an `IOMGR_Select` transferring control to the IOMGR thread,
which then blocks on a UNIX select.  On receiving a response, the IOMGR thread
unblocks from the select, wakes up the SocketListener thread, which receives
and decodes the response and then hands control back to the client thread.

At the server, the server thread blocks waiting for a request, hence
transferring control to the SocketListener.  As with the client, the
SocketListener calls `IOMGR_Select` and blocks, thereby handing control to the
IOMGR thread.  When a request arrives, the IOMGR thread unblocks from the
select, hands control to the SocketListener which after receiving and decoding
the packet transfers control back to the server thread.

The IOMGR thread has the lowest priority.  All other threads are usually at
(the same) higher priority level.

#### RPC2\_Init

- By calling `LWP_Init`, this routine converts the calling process into the
  initial thread thereby creating the client thread at the client end and the
  server thread at the server end.

- Creates and sets up an Internet socket through which requests  are sent
and responses received.

- Creates the SocketListener thread using the `LWP_CreateProcess` primitive.

- Initializes the IOMGR package, thereby creating the IOMGR thread.

- By calling `TM_Init` initializes the `rpc2_TimerQueue`[^1] list which is then
  used to maintain all the timers for the RPC calls.

- Initializes the random number generator used for generating sequence numbers
  for packets.

- Sets the retry parameters according to the algorithm described in [Failure
  Detection](failure.md).

[^1]: `RPC2_xxx` are the routines that are externally visible and can be used
      by the user.  `rpc2_xxx` are routines that are internal to the library.

### Data Structures used in RPC2

A number of data structures are used in RPC2, the most important of which
are the Connection Entry (CEntry)  the `RPC2_PacketBuffer`, and the
SocketListener Entry (SLEntry).  We consider each one of these in turn.

#### CEntry

RPC2 provides logical connections for the client to communicate with
the server.

Associated with each logical connection are two data structures of type CEntry;
one at the client end and the other at the server end.  All information
relevant to the connection is entered into these data structures.  This
includes information about the security of the connection,  the state and
identity of the connection itself, the remote site that it is connected to, the
sequence number of the next packet expected on the connection and so on.

The CEntry is allocated using the `rpc2_AllocConn` call.

#### RPC2\_PacketBuffer

The `RPC2_PacketBuffer` shown in *rpc2.h*,  defines the structure of the
request-response packets exchanged during the RPC calls.  The
`RPC2_PacketBuffer` consists of a Prefix, Header and Body.  The Prefix is of
fixed length and contains information about the PacketBuffer.  It is used
internally by the runtime system, and is not transmitted.

The Header is also of fixed length, and contains a number of fields some of
which are described below.  The fields pertaining to the connection are the
RemoteHandle and LocalHandle.  The RemoteHandle and the LocalHandle refer to
the connection ids on the peer host and the local host respectively. The
LocalHandle field corresponds to the local connection id on which the packet is
sent.  In this case, the LocalHandle in  the request packet corresponds the
clients local connection id.  In the response packet this field corresponds to
the servers local connection id.

Other fields pertain to the packet itself including the length of the packet,
the sequence number, the type of packet (determined by the Opcode), the length
of the packet.  These fields are encrypted on secure connections.  The packet
length field is used by the peer host to efficiently allocate packet buffers.
The Body is of arbitrary size, and is used to transmit the input and output
parameters of the RPC.

The packet buffer is allocated by calling `RPC2_AllocBuffer` and is freed by
`RPC2_FreeBuffer`.

#### SLEntry

SLEntries are data structures used for communication between the user LWPs and
the SocketListener.

There are three types of SLEntry data structures; REPLY, REQ, OTHER.  Each of
these are described below.

REQ

:   The SLEntry of type REQ is used by the server in the `RPC2_GetRequest` call
    (which is described later).  In the `RPC2_GetRequest` the server thread
    allocates a SLEntry, associates a filter and timeout with it, activates it
    and goes to sleep until the SLEntry is modified.  The SLEntry is modified
    by the SocketListener when a packet arrives  or a timeout occurs.  A return
    code associated with the SLEntry is used to indicate if the timeout
    occurred or a request arrived.

OTHER

:   The SLEntry of type OTHER is associated with a specific connection.  It is
    created and destroyed by the user LWP.  Typically it is used by a client
    making a request on a particular connection.  The client allocates a
    SLEntry of this type.  A timeout equal to the retry parameter is associated
    with the SLEntry.  The client then activates it and goes to sleep until the
    SLEntry is modified.  As with the REQ type SLEntry this modification is
    done by the SocketListener when either a packet arrives, a BUSY is received
    or a timeout occurs.  The SLEntry is then deactivated with the appropriate
    return code.  If there is an overall timeout associated with the call (as
    specified by the user in the `RPC2_MakeRPC` call), before the client goes
    to sleep, it allocates, sets this timeout value and activates another
    SLEntry of type OTHER.

REPLY

:   The SLEntry of type REPLY is associated with a specific connection, is
    created by the user (server) LWP in the `RPC2_SendResponse` call, and is
    deactivated by the SocketListener in the `rpc2_ExpireEvents` call.

The type of SLEntry is specified in the field *Type* of the SLEntry
structure.

The SLEntries are allocated using the `rpc2_AllocSle` call.  They are activated
and deactivated using the `rpc2_ActivateSle` and `rpc2_DeactivateSle` calls
respectively.  They are freed using the `rpc2_FreeSle` call.

#### Creating Data Structures

To avoid the cost of allocating a structure each time it is required, lists are
maintained for each of the three mentioned data structures.  These lists are
called FreeLists (SLFreeList for SLEntries, ConnFreeList for CEntry,
`rpc2_PBList` for PacketBuffers), and contain the addresses of entries which
were created previously, but are no longer in use.

These lists are first checked for a free entry before mallocing space.  When an
entry is no longer required it is returned to the respective FreeList.

### Binding

In order for the client to communicate with the server, it is first necessary
to establish a logical connection between them. These connections are
established using the `RPC2_Bind` primitive.

#### RPC2\_Bind[^2]

[^2]: Here we only consider the unauthenticated and unencrypted security level
      *RPC2_OPENKIMONO*.

- Creates a new connection and binds to a server on a remote host.  The
  identification of the server is specified by a hostname, port number and
  subsystem.  This along with the required security level is mapped into a
  unique integer which we will refer to as the *Local Handle*.  The Local
  Handle can also be thought of as the connection identifier, and is  used by
  the client for future references to this connection.

- Creates  a data structure of type  CEntry by calling `rpc2_AllocConn`.  The
  Local Handle is entered into CEntry field *UniqueCID*.

- Allocates and packs into a packet buffer, the information required by the
  server to set up the connection.

- Allocates and activates a SLEntry data structure of type OTHER.

Next, the `RPC2_Bind` packet is sent to the server.  The details of the calls
required to send a packet will be discussed later while describing the
`RPC2_MakeRPC` call.  After sending the packet, the client thread goes to
sleep.

The server responds to this call by creating a corresponding connection entry.
When the SocketListener thread detects a packet, it receives it and processes
it.  Processing involves sanity checking and decoding the packet.  This packet
is decoded as a Bind request.  The SocketListener thread allocates a CEntry
data structure, and a unique connection identifier (using the `rpc2_AllocConn`
call).  Information relevant to this connection such as the clients id,
subsytem id etc is entered into CEntry by calling `rpc2_MakeConn`. The server
then allocates a Packet Buffer, enters the connection identifier in the Local
Handle field of the Packet Header, and sends the response to the client.  The
connection identifier  is used by the client in subsequent requests to the
server on this connection.  All calls associated with the server will be
discussed later.

At the client end, the SocketListener thread detects a packet,  receives and
processes it.  Processing involves sanity checking and decoding the packet.
This packet is decoded as the response to the Bind packet, and the connection
identifier sent by the server is entered into the CEntrys field PeerHandle.

Now the connection has been set up.

### Client-Related RPC Calls

A client wishing to make a remote procedure call on this connection makes a
local procedure call to the client stub and specifies the connection (or Local
Handle) on which it wishes to make a call.  The client stub allocates a packet
buffer, marshalls the arguments into a packet and calls `RPC2_MakeRPC`.

#### RPC2\_MakeRPC

- Obtains the connection entry associated with the corresponding Local Handle.
  Initializes the request packet buffer by filling in the appropriate header
  information using `rpc2_InitPacket`.

- Allocates a SLEntry of type OTHER.

- Sends the request packet by calling the `rpc2_SendReliably` primitive.

#### rpc2\_SendReliably

- Transmits the request packet using the `sendto` system call, and activates
  the SLEntry.  The SLEntry is activated using `rpc2_ActivateSle` where the
  timeout value is set to the retransmission timeout $b_1$. The entry is placed
  in the timer list managed by the TM package.

- The client thread then blocks until either a response is received or a
  retransmission timeout occurs. When the client thread unblocks, it first
  checks to see if the overall timeout of the call has expired (i.e the timeout
  specified in `RPC2_MakeRPC`).  If this occurs, then this timer is deactivated
  and removed from the list and the call returns to `RPC2_MakeRPC`.  If the
  call has not timed out, the return code on the SLEntry is used to find out
  the outcome of the call. There are four possibilities:

  - A negative acknowledgement (NAK) is received in which case control is
    transferred back to the `RPC2_MakeRPC` call.
  - A response is received in which case control is transferred back to the
    `RPC2_MakeRPC` call.
  - A BUSY response is received.  A BUSY is sent to a client by the server in
    response to clients retries.  This is to indicate to the client that the
    server is still alive and connected to the network.  In this case the
    SLEntry is again activated with the retransmission timeout set to $b_0$.
  - A retransmission timeout occurs. The client calls `rpc2_CancelRetry` to
    check if a side effect has heard from the server during the interval.  If
    this is the case, then the client behaves as if it received a BUSY at the
    time of the last server response in the side effect.  In other words,
    suppose while the thread was blocked the client received a packet from the
    server as part of the execution of a side effect.  Let that time be $t$.
    Suppose the time is now $n$.  Then the SLEntry is reactivated with the
    retransmission timeout set to $b_0 - (n - t)$. If the client has not heard
    from a side effect in the interval, then the SLEntry is again reactivated
    with the retransmission timeout set to the next value of $b$, until $N$
    retries have been attempted.  After $N$ retransmissions, the server is
    assumed dead and the call transfers control back to `RPC2_MakeRPC`.

- On returning to the `RPC2_MakeRPC` call, the return code indicates:

  - If an overall timeout occurred (i.e. the return value of
    `rpc2_SendReliably` is *RPC2_TIMEOUT*, `RPC2_MakeRPC` frees the
    corresponding SLEntry and returns control to the client stub with the
    outcome of the RPC call (i.e. the return code) specified as *RPC2_TIMEOUT*.
  - If an overall timeout did not occur (i.e. the return value of
    `rpc2_SendReliably` is *RPC2_SUCCESS*, then the response to the RPC call
    could be one of three.  The ReturnCode field in the SLEntry indicates which
    of the three cases occured.

    - The response was received, indicated by return code ARRIVED.  Control is
      transferred to the client stub with the return code of the RPC call set
      to *RPC2_SUCCESS*.
    - A negative acknowledgment, indicated by return code NAKED.   Control is
      transferred to the client stub with the return code of the RPC call set
      to *RPC2_NAKED*.
    - No response because of either the server being dead or the network being
      down, indicated by return code TIMEOUT.   Control is transferred to the
      client stub with the return code of the RPC call set to *RPC2_DEAD*.

- In all cases the SLEntry is freed and control is transferred back to the
  client stub.  The arguments of the call are unmarshalled and returned to
  the calling program.

### Server Related RPC Calls

The server program is given in [`rtime_srv.c`](examples/rtime_srv.c.md). After
initialization, in order for the client to bind to a server, the server must
first export the subsytem.  This is done using the `RPC2_Export` primitive.
The server  now ready to receive requests, makes a call to `RPC2_GetRequest`.

#### RPC2\_GetRequest

- First, checks to see if there are any *HeldRequests*.  When a request
  arrives, and there are no threads available to service it, it is added to a
  *HoldList*.  We refer to these requests as HeldRequests.  If there are no
  HeldRequests, it looks for new requests.  This is done by making a call to
  *GetNewRequest*, which allocates and activates a SLEntry of type REQ, and
  blocks until either a timeout (specified in `RPC2_GetRequest` occurs or
  a request arrives.

- If the request is a Bind request, the SocketListener thread after allocating
  a CEntry using the `Make_Conn` call hands control to the server thread.
  The server then checks to see that it is a bind request.  It then allocates a
  PacketBuffer and transmits the Bind packet (called the Init2 packet) to the
  client.  This is done by calling the `SendOKInit2` primitive.

- If a request arrives, on an already existing connection, control is
  transferred back to the main server program which then transfers control to
  the server stub.  The request is then executed, a PacketBuffer is allocated
  and the response is sent to the client using the `RPC2_SendResponse` call.

#### RPC2\_SendResponse

- Takes as input the Local Handle from which it obtains the corresponding
  CEntry using the `rpc2_GetConn` call.

- Records the relevant information into the response Packet Header and
  transmits the response packet using `rpc2_XmitPacket`.

- Allocates and saves a packet for retry which is sent to the client if a
  duplicate request is received.

### TM related RPC2 calls

In this section we describe some of the frequently used RPC2 primitives that
deal with the calls in the TM package.

#### rpc2\_ActivateSle

- Sets the fields of the timer element *TM_Elem*; sets the TotalTime field with
  the timeout value, sets the BackPointer field with the address of the
  SLEntry.   If there isnt a timeout associated with the SLEntry, the TotalTime
  field is set to -1.

- Checks to see if the timeout of the new *TM_Elem* is less than any of those
  already in the list (the timeout field of the *TM_Elem* is compared with the
  TimeLeft fields of all the other *TM_Elem*s in the list). If so, the
  SocketListener thread is made to wait on this *TM_Elem*.

- Inserts the timer element into the timer list *rpc2_TimerQueue*.

#### rpc2\_DeactivateSle

- Enters the specified return code to ReturnCode field of the SLEntry.  The
  ReturnCode can be one of of the following.

    TIMEOUT
    :   if the TimeLeft field of the TM&lowbar;Elem is less than or equal to 0.

    ARRIVED
    :   if the response has arrived.

    KEPTALIVE
    :   if a BUSY response is received.

    NAKED
    :   if a negative acknowledgement is received.

- If *TM_Elem* is timed (i.e TotalTime field is not equal to -1), it removes it
  from Timer list.

Note that all SLEntries are deactivated by the SocketListener.

#### rpc2\_ExpireEvents

This routine is called by the SocketListener.

- Using `TM_Rescan`, it updates the TimeLeft fields on all the timers in the
  *rpc2_TimerQueue*.  It then checks for expired timers and deactivates the
  SLEntries with the expired timers.

### SocketListener

An integral part of the RPC2 code is the  SocketListener.  As previously
mentioned, the SocketListener thread monitors the IP Socket through which
packets are sent and received.  A SocketListener thread is created at the
client and the server  when RPC2 is initialized.   Although the functionality
of the SocketListener at the client and at the server are similar, some
differences do exist.  We first discuss the section of the code that is common
to both.

- On creation, the SocketListener thread yields to the main thread (the client
  thread for the client and server thread for the server).  It gains control
  when the main thread goes to sleep.

- It first polls the IP socket (polling select) to see if any packets are in
  the socket. This is done using the `MorePackets` primitive.

- If there are no packets waiting in the socket, by calling `rpc2_ExpireEvents`
  it checks the Timer list to see if there are any expired events. Expired
  events refer to  timeouts that might have occurred  since it was last
  checked.

- It then finds the earliest event on the Timer list, and blocks on an
  `IOMGR_Select` until either a packet arrives or the event times out.  This is
  done by calling `PacketCame`.

- If a packet is present in the socket, the SocketListener processes the packet
  by calling `rpc2_ProcessPacket`.  The `rpc2_ProcessPacket` routine calls on a
  number of primitives and is described below.

#### rpc2\_ProcessPacket

- The `PullPacket` primitive allocates a Packet Buffer for the incoming packet.
  The packet is then received using the `rpc2_RecvPacket` call which in turn
  uses the `recvfrom` system call.

- `PoisonPacket` is called to sanity check the packet.  This entails checking
  the version of RPC2 used, and the packet header.

- The RemoteHandle field of the packet is used to determine if the packet is a
  new bind request.  If not, the CEntry is obtained  using the `rpc2_GetConn`
  call.  Once the CEntry is obtained the packet is decoded by calling
  `DecodePacket`.

#### DecodePacket

Here is where the differences arise between the client and the server.  We
first consider the server and then the client.

- For a server, the packet could be a request for a new connection, a new
  request, or a retransmission .  A retransmission could occur because the
  response packet was lost or the server is busy.  In the first case, the
  response is retransmitted.  If the server is busy, a BUSY packet is sent out.
  The reason for having the BUSY mechanism is the following.

- When the server is heavily loaded, or a long side effect (such as a large
  file transfer) is occurring, responses to clients requests may take an
  arbitrary time.  In the meantime however the client might timeout and
  retransmit.  After a number of retransmissions, if no response is received,
  the connection is declared to be dead.  To prevent this from happening, the
  server sends out an **RPC2_BUSY** if it sees a retransmission, thereby
  informing the client that it is still alive.  When the client receives an
  **RPC2_BUSY** it backs off from retransmitting for time $b_0$.  Another
  advantage of the BUSY is that it prevents clients from flooding the server
  with retransmissions, and hence decreases unnecessary load on the server.

- If the packet is a bind request,  a CEntry is created.  The SocketListener
  thread then checks if any server threads are available.  If so it deactivates
  the corresponding SLEntry.  If not it puts the request in the  HeldRequest
  queue.  Similarly for the new request, it checks for available server threads
  and depending on the availability it either deactivates a SLEntry or puts the
  request on a HoldList.

- At this point the SocketListener thread does not yield control.  It polls to
  see if any packets are waiting in the socket.  If so, it processes them, if
  not it checks on expired events and blocks on the `IOMGR_Select` and then
  yields to the server thread.  The rationale for the server thread not
  yielding control immediately after deactivating the SLEntry is as follows. If
  retransmissions are present, it is generally better to send out the BUSY
  responses as soon as possible. Hence the SocketListener thread decodes all
  the packets waiting in the socket and responds to the retransmissions before
  yielding control.

For a client, the packet could be a response to a bind request, a response to a
request or a BUSY from the server or a NAK.  For each of these, the SLEntry
waiting on the event is found and deactivated by calling `rpc2_DeactivateSle`.
As in the case of the server, at this point the SocketListener does not yield
control.  It polls the socket to see if any packets are waiting in the socket.
If a packet is waiting it processes the packet, otherwise it checks on expired
events, blocks on an `IOMGR_Select` and then yields control to the client
thread.
