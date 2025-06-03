# SFTP Internals

## Background

An SFTP file transfer can take place from either an RPC2 server or a RPC2
client. To avoid confusion we will refer to the transmitting entity as the
*source* and the receiver entity as the *sink*.  RPC2 clients and servers are
not regarded as peers. While an RPC2 client might be someones personal
workstation, an RPC2 server could be serving a large user community. In an
effort to improve scalability when more clients are added to the system, the
servers will handle all SFTP flow control, irrespective if they are the source
or the sink.

An RPC2 client can use SFTP to transfer a file simultaneously to more than one
RPC2 server using IP multicasting. Multicast file transfers are only possible
when the source is an RPC2 client. The sinks will send flow control information
to the source, and it will adapt to the requirements of the slowest sink.

An SFTP file transfer is basically a cyclic exchange of data and
acknowledgements. At the beginning of each cycle, the source will send a block
of data packets. It will then wait for an acknowledgement to arrive. The
acknowledgement will specify which packets the sink has received. The cycle
then repeats. The source will now retransmit any packets that it knows that the
sink did not receive, followed by a block of new packets.

When the source has transmitted a block of data packets, it will wait for the
arrival of an acknowledgement. If the source is an RPC2 server and the
acknowledgement does not arrive after a predetermined time, the source will
retransmit the block of data packets. It basically acts as if it received an
acknowledgement that indicated that the entire block of data packets had been
lost.

If the source is an RPC2 client, however, it will wait passively for an
acknowledgement to arrive. If the sink does not receive more data packets after
a predetermined period of time, it will conclude that acknowledgement was lost
in transit and retransmit it.

## SFTP Code Structure

In this section we describe the SFTP code structure.  Our description assumes
that the reader is already familiar with the description of basic [RPC2
Internals](rpc2_internals.md).

### Thread creation and Initialization

In the base RPC2, the RPC2 client and server communicate via Internet Sockets.
Both at the client and at the server, the socket is created during
initialization by calling `RPC2_Init`.  A SocketListener thread is present at
both ends to monitor these sockets.

When using SFTP, in addition to the above, there is another set of sockets
created; one at the client and another at the server.  These sockets are
monitored by the `SFTP_Listener`.  Both of these are created during the
iniailization of the SFTP package.

Note that there are two independent channels of communication between the
client and the server.  The first channel (which we will refer to as the *RPC2
channel*) that is associated with the base RPC2 is used for making simple RPCs,
for making RPCs requesting for the file transfers, for retransmissions and
BUSYs.  All other exchanges related to the file transfer are handled by the
second channel  which will be referred to in future as the *SFTP channel*.

As previously mentioned, a file can be transferred from a client to a server or
from a server to a client.  In the first case, the server is the sink.  In the
second case, the client is the sink.  The code however is not symmetric; i.e.,
the code executed when a client is the sink is slightly different from the code
executed when a server is the sink.  We describe both cases below.

### Data Structures used in SFTP

In addition to the data structures used in RPC2 (on the RPC2 channel), SFTP
uses a data structure called `SFTP_Entry` which is given in `sftp.h`.  It
contains fields  relevant to the SFTP channel such as the *LocalHandle*.
Other fields include the state of the file transfer, the packet size, the
window size and a number of others.  It is created by the call
`sftp_AllocSEntry`.

### File transfer from server to client

This is the case in which the server is the source and the client is the sink.
The client makes a request for the file by doing a `RPC2_MakeRPC` on the RPC2
channel.

This is received by the servers SocketListener which wakes up a suitable
LWP blocked on a `RPC2_GetRequest`.  The LWP then calls the routine that is
meant to handle this request.  This routine contains calls to two routines,
`RPC2_InitSE` and `RPC2_CheckSE`.

`RPC2_InitSE` initializes certain internal data structures. `RPC2_CheckSE`
handles the actual file transfer.  The main procedure in `RPC2_CheckSE` which
deals with the file transfer from SERVERTOCLIENT is the `PutFile` routine.

The `PutFile` routine sets some of the fields of the data structure SEntry,
sets the transfer state of SEntry to *XferInProgress* (transfer in progress)
and calls `sftp_SendStrategy`. This routine sends a set of packets, using a
strategy described in the next section.

After sending the first set of packets, a *while* loop is entered and executed
as long as the transfer state of SEntry is still in *XferInProgress*.  In the
*while* loop, `AwaitPacket` and `sftp_SendStrategy` are called alternately.
The `AwaitPacket` routine waits for either an ACK, NAK or for a timeout.  If a
timeout occurs, the packets are retransmitted using `sftp_SendStrategy`.  If an
**SFTP_ACK** is received, the `sftp_AckArrived` routine is called.  This
routine advances the transmission window and checks to see if the transfer is
complete.  If so, it sets the SEntrys transfer state to *XferCompleted*, and
the *while* loop is exited.  Otherwise, the next set of packets is transmitted,
after which control is yielded.

Note that all these packets are sent on the SFTP channel, not the main
RPC2 channel.

At the client end, the `sftp_Listener` detects a packet in the socket, receives
it and processes it by calling `sftp_ProcessPacket`.  This routine after
receiving the packet calls the `ExaminePacket` routine.  This routine sanity
checks the packet, and identifies it as a **SFTP_DATA** packet.  It then calls
`sftp_DataArrived` which sends the requested ACKs and writes the data to disk
by calling the `WriteStrategy` routine.  The `sftp_Listener` yields after each
packet it processes.

The packet from the client is received by the `sftp_Listener` at the server
which then calls the `ServerPacket` routine which modifies the appropriate
SEntry.  It then does an `IOMGR_Select`, and yields control.  Control is then
transferred to the LWP waiting on this packet, and the cycle continues.

### File transfer from client to server

This is the case in which the server is the sink and the client is the source.
As in previous case, the client makes a request for the file by doing a
`RPC2_MakeRPC` on the RPC2 channel. This is received by the servers
SocketListener which wakes up a suitable LWP blocked on a `RPC2_GetRequest`.
The LWP then calls the routine that is meant to handle this request.  This
routine contains calls to the two routines `RPC2_InitSE` and `RPC2_CheckSE`.
The `RPC2_InitSE` initializes some of the fields of the data structure.  The
main routine in `RPC2_CheckSE` which deals with the file transfer from
CLIENTTOSERVER is the `GetFile` routine.

The `GetFile` routine sets some of the fields of the data structure SEntry,
sets the transfer state of SEntry to *XferInProgress* and sends a
**SFTP_START** packet to the client to tell the client that the server is ready
to receive the file.  It then enters a *while* loop which is executed as long
as the transfer state of SEntry is still in *XferInProgress*.  In the *while*
loop, `AwaitPacket` and `sftp_DataArrived` are called alternately.  The
`AwaitPacket` routine waits for either a packet to arrive or for a timeout.  If
a timeout occurs, the **SFTP_ACK** is retransmitted.  If a **SFTP_DATA** packet
is received, the `sftp_DataArrived` routine is called.  This routine in turn
calls the `sftp_WriteStrategy`.  When the file transfer is eventually
completed, the transfer state of the SEntry is set to *XferCompleted*, and the
loop is exited.

The `sftp_Listener` at the client end receives the packet and decodes it, and
calls the `ClientPacket` routine which in turn identifies the packet as an
**SFTP_START** packet.  It then calls `sftp_StartArrived` which sets some of
the fields in the SEntry data structure and calls the `sftp_SendStrategy`
descirbed above.  The `sftp_Listener` then blocks on an `IOMGR_Select`.  Note
that it patiently waits for an **SFTP_ACK** from the server, and does not
retransmit if it does not receive an **SFTP_ACK** within a given time.  What
prevents the client from waiting forever is that communication exists between
the client and the server on the RPC2 channel in the form of retransmissions
and BUSYs.  When an **SFTP_ACK** arrives, it transmits the next set of packets.

The `sftp_Listener` at the server end receives the packet and calls the
`ServerPacket` routine.  This routine wakes up the appropriate LWP which is
blocked in the `AwaitPacket` call.[^1]

[^1]: Note that although the client sends a number of packets, the
      `sftp_Listener` receives and processes them one at a time; yielding
      control after each one.  The same applies at the server end.

Note that the role of the `sftp_Listener` is different at the client end and at
the server end.  At the client end, the whole sftp transfer is handled by the
`sftp_Listener`.  At the server end the `sftp_Listener` receives and decodes
the packet.  Most of the sftp transfer is handled by the LWP thread.

## Packet formats

All packets carry 32 bit sequence numbers. Data packets and control packets
have independent sequence numbers. The sequence number series of the source and
sink(s) are also independent of each other.

There are thus at least 4 sequences in a connection:

- Source to Sink, Data
- Source to Sink, Control
- Sink to Source, Data
- Sink to Source, Control

The Sink to Source sequence space is currently never used. When doing a
multicast file transfer each sink will have independent sequence number series.

The sequence number for a particular packet type is incremented by one for each
new packet of its type that is sent.

The *MOREDATA* flag will be set in each data packet except for the very last
one. This is to facilitate end of file detection.  If the *ACKME* flag is set
on a data packet it requests an acknowledgement, **SFTP_ACK**, from all of the
servers.

Each **SFTP_ACK** packet describes which packets have been received by the
particular server. There should be little or no need to transmit an
acknowledgement packet for each data packet. It is of particular benefit to
limit the number of **SFTP_ACK** packets given our single channel operating
environment. The acknowledgement packets will contend with data packets going
in the other direction.

Each acknowledgement packet has a 64-bit wide bitmask and an offset counter,
*GotEmAll*. This counter is the highest sequence number of a data packet such
that it and all preceding data packets have been received. The bitmask
indicates which of the data packets with sequence numbers greater than
*GotEmAll* that have been received.  Each bit in the bitmask represents a
single packet.

### Protocol details

If the source is an RPC2 client it must first wait for permission from
the sink (s) before it can transmit. This permission is granted by a
special **SFTP_START** packet.

The following counters are of relevance to the SFTP source protocol machine.
*SendLastContig*, which is the sequence number of the latest packet to be moved
out of the transmission window, and *SendMostRecent*, which is the sequence
number of the data packet last sent. There are also three important
transmission parameters: the transmission window size, *AckPoint*, and size of
the *SendAhead* set.

When an SFTP source begins the transfer, *SendLastContig* and *SendMostRecent*
will be equal. The packets in the *SendAhead* set are transmitted, and
*SendMostRecent* is increased by the size of *SendAhead*. Only one of these
packets will have the *ACKME* flag set. The relative position of this packet in
the *SendAhead* set is given by *AckPoint*. *AckPoint* must thus be less than
or equal to the size of the *SendAhead* set.

Packets which have been sent and for which an **SFTP_ACK** has been requested
but not yet received fall into two categories: the *NeedAck* set and the
*Worried* set.  They are distinguished by whether or not an retransmission
timeout has occurred since they were sent. Packets in the *NeedAck* set have
been sent and an **SFTP_ACK** has been requested, but not enough time has
passed to be worried about the fact that an **SFTP_ACK** has not been received.
Packets which have been sent for which an **SFTP_ACK** has not yet been
requested, if any, are called the *InTransit* set. The *InTransit* set will
always be empty if *AckPoint* equals the *SendAhead* size.

The source then waits for an **SFTP_ACK** packet from the sink.  Our
implementation uses the waiting time to prefetch more data from the disk.
During ideal conditions the source will proceed only after having received the
**SFTP_ACK** it is waiting for.  In practice, however, it may timeout and
retransmit data packets if it is operating as an RPC2 server.

At this point, the source will revise the *Worried* set. Any packets that have
been acknowledged will be taken off the *Worried* set.  The transmit window is
shifted by increasing the *SendLastContig* counter. It will be set to one less
the smallest sequence number of a member in the *Worried* set.

All the packets that are in the *Worried* set are retransmitted followed by
*SendAhead* new packets. No packets will have the *ACKME* flag set, except for
the member of *SendAhead* set whose index is given by *AckPoint*. The packets
in the new *SendAhead* set are then either added to the *NeedAck* set or to the
*InTransit* set, depending upon the *AckPoint* value, as described above.
Packets are placed in the *Worried* set only after a retransmission interval
has expired.  The procedure is repeated until the file has been completely
transfered. At no point, however, will the protocol have more packets
outstanding than what is given by the transmit window size. Whenever the sum of
the number of packets in the various sets is greater than the transmit window
size, only the first packet in the *Worried* set will be sent.

### Sink side operation

The sink keeps two counters, *RecvLastContig* and *RecvMostRecent*, which are
similar to their counterparts at the source side. *RecvLastContig* is the
sequence number of the last data packet where it and all previous data packets
have been received. It is used as the *GotEmAll* counter when sending an
**SFTP_ACK** packet. *RecvMostRecent* is the highest sequence number of a
packet received so far.

Before the file transfer takes place, the source will inform the sink about the
parameters *RetryInterval*, *RetryCount* and *DupThreshold*.  If the sink is an
RPC2 server it will grant the source permission to transmit data packets by
sending a **SFTP_START** packet. It will then start waiting for data packets.
If the sink is an RPC2 server and no data packet has arrived after the time
specified by *RetryInterval*, it will send an **SFTP_ACK** (or **SFTP_START**)
packet, trying to cause the source to retransmit its data. If this fails
*RetryCount* number of times, without any valid data packet being received, the
sink will consider the connection unusable.

The sink keeps track of the number of duplicate data packets that have arrived
since the last time an **SFTP_ACK** was sent. If that number exceed
*DupThreshold*, the sink will send an **SFTP_ACK** in an attempt to inform the
source about the situation.

### Client and server invariants

The state of the counters at the source and sink can be summarized by the
following invariant relations, where *SendAckLimit* and *SendWorriedLimit* are
upper bounds of the *NeedAck* and *Worried* sets, respectively.

#### Invariants when transfer is in progress

 1. SendLastContig &lt;= SendWorriedLimit &lt;= SendAckLimit &lt;= SendMostRecent

    (SendMostRecent - SendLastContig) &lt;= WindowSize

    (SendMostRecent - SendAckLimit) &lt;= SendAhead

 1. RecvLastContig &lt;= RecvMostRecent

    (RecvMostRecent - RecvLastContig) &lt;= WindowSize

#### Invariants when transfer is completed, aborted or not started

1. SendLastContig (at source) = SendMostRecent (at source)
1. RecvLastContig (at sink) = RecvMostRecent (at sink)
1. SendLastContig (at source) = RecvLastContig (at sink)

## Adjusting the Retransmission Interval

SFTP uses the retransmission interval to determine when it should be worried
about packets for which it has not received an **SFTP_ACK**.  Initially, the
retransmission interval is set to `SFTP_RetryInterval` (2 seconds), but varies
depending on RTT observations collected during file transfers.  When a timeout
occurs, the retransmission timer is backed off.  The backoff timer is
independent of the RTT state in the *sEntry*.

Like RPC2, SFTP collects RTT observations by using packet timestamps.  The
timestamps and storage of RTT state is the same as presented in [Failure
Detection](failure.md).  In SFTP, timestamping is *two-way*, namely, both
source and sink collect RTT observations during a transfer. Both timestamp
fields in the packet header are used: one for the current timestamp, and one
for the timestamp being echoed back to the other side.

The source collects observations as follows: it timestamps outgoing
**SFTP_DATA** packets.  The sink echos a timestamp back on the **SFTP_ACK**
packet. When the **SFTP_ACK** arrives at the source, the source computes the
RTT for that send-ahead set, and updates its RTO accordingly.

The sink collects observations as follows: it timestamps **SFTP_START**,
**SFTP_ACK**, and **TRIGGER** packets. (A trigger packet is an **SFTP_ACK**
that is being used by the server because it has timed out on the client.) The
source echos the timestamp back on the first **SFTP_DATA** packet sent in
response to such a packet. When that **SFTP_DATA** packet arrives at the sink,
the sink computes the RTT and updates its RTO.  If the first packet gets lost,
no update is performed. If it is delayed, the update is performed when it
arrives.

All that is needed for state in the *sEntry* is a single word, *TimeEcho*, to
hold the timestamp that will next be echoed on a packet.  Each packet may carry
up to two timestamps - one is the time at which the sender sent it, and the
other is the echoed timestamp. (Only **SFTP_ACK** packets and certain
**SFTP_DATA** packets actually use both fields.) The spare2 and spare3 fields
of the packet header are used for these fields, called *TimeStamp* and
*TimeEcho*.  These fields were previously reserved for bitmask fields, but were
not being used.

Packets are timestamped as they are sent out in the following routines:

- `sftp_SendSendAhead`, `sftp_ResendWorried`, `sftp_SendFirstUnacked` (data)
- `sftp_SendAck`
- `sftp_SendStart`

The packet TimeStamp field is stashed in `sEntry->TimeEcho` as appropriate
when a packet with a timestamp is received. This is the timestamp that
will be echoed back to the other side eventually. This occurs in:

- `sftp_DataArrived`, on the sink, if the packet advances the left edge of the
  window (`Header.SeqNumber == sEntry->RecvLastContig+1`).
- `sftp_StartArrived`, on the source, whether the transfer has started or not.
  Data will be sent in response to the **SFTP_START** packet either way.
- `sftp_AckArrived`, on the source. If there is more data to send, the source
  will send it in response to this packet.

The value in `sEntry->TimeEcho` is then placed in the `Header.TimeEcho` field
in the following routines:

- `sftp_SendAck`, on the sink.
- `sftp_SendSendAhead`, `sftp_ResendWorried`, or `sftp_SendFirstUnacked`, on
  the source. The timestamp is echoed on the *first* packet sent out by these
  collectively (the one corresponding to `sEntry->SendLastContig+1`).  All
  other **SFTP_DATA** packets carry a TimeEcho of 0. A special case occurs in
  the first set of **SFTP_DATA** packets on a server-to-client transfer, from
  PutFile.  In this case there is no timestamp to echo, because the source does
  not hear from the sink before sending data. In this case, `sEntry->TimeEcho`
  is set to 0 at the top of PutFile. A second special case also occurs in
  PutFile, when the server times out. Again, there is no timestamp to echo,
  because the data is not being sent in response to a packet from the sink.

RTT measurements are computed from Header.TimeEcho in the following routines:

- `sftp_AckArrived`, on the source, if the **SFTP_ACK** is not a trigger.
  Triggers are sent when the server times out during a client-to-server
  transfer. They do not represent real observations because there was no
  transmission from the source that caused them. Triggers are marked so that
  the source can distinguish them from real **SFTP_ACK**s.
- `sftp_DataArrived`, on the sink.

Any zero TimeStamp or TimeEcho is ignored, and the RTO remains unchanged. This
is chiefly for compatibility with versions of SFTP that do not use packet
timestamps.  RTT state in the sEntry is initialized on the client in
`SFTP_Bind2`, using the BindTime supplied by RPC2. On the server, it is
initialized in `SFTP_GetRequest`, using the same BindTime shipped to the server
on the first request on the connection.

## Performance

RPC2 and SFTP perform well over a wide range of network speeds.
The following table compares the performance of SFTP and TCP over
three different networks: Ethernet, a WaveLan wireless network, and a
modem over a phone line.  In almost all cases, SFTPs performance
equals or exceeds that of TCP.

| Protocol | Network | Nominal Speed | Receive (KB/s) | Send (KB/s) |
| -------- | ------- | ------------- | -------------- | ----------- |
| TCP | Ethernet | 10 Mb/s | 228 (8) | 300 (28) |
| TCP | WaveLan | 2 Mb/s | 71 (17) | 95 (10) |
| TCP | Modem | 9.6 Kb/s | 0.85 (0.008) | 0.80 (0.005) |
| SFTP | Ethernet | 10 Mb/s | 244 (13) | 343 (12) |
| SFTP | Wavelan | 2 Mb/s | 144 (8) | 146 (6) |
| SFTP | Modem | 9.6 Kb/s | 0.82 (0.002) | 0.86 (0.003) |

/// table-caption
Transport Protocol Performance

This table compares the observed throughputs of TCP and SFTP.  The data was
obtained by timing the disk-to-disk transfer of a 1MB file between a DECpc
425SL laptop client and a DEC 5000/200 server on an isolated network.  Each
result is the mean of five trials.  Numbers in parenthesis are standard
deviations.
///
