# Failure Detection

The hazards facing the user of an RPC package:

1. The communication medium may fail.
1. The peer process at a remote site may crash.

A key problem in RPC is reliably detecting either of these events when an RPC
call is in progress.  Detection of failures in the absence of RPC calls in
progress is an orthogonal issue, and can be reduced to this issue by generating
artificial keepalive RPC calls.

Ideally, the detection of these failures should be independent of the specific
RPC call in progress.  In other words, as long as we are sure that
communication medium is not broken and that the remote server process is alive,
we should not care how long it takes to receive the reply to an RPC request.
At the same time failures should be detected as soon as possible, so that
suitable recovery actions can be performed.  The following paragraphs show this
goal is achieved in RPC2.

## Retransmission Algorithm

When the RPC2 runtime system receives a retry packet for a request it is
already working on, it responds with a **Busy** packet.  There are two
constants $B_{total}$ and $N$.  These constants are set in `RPC2_Init()`, with
suitable defaults built in.  The semantics of these two constants are:

1. Communication failure is declared if $N$ successive retries of a packet fail
   to provoke any kind of response.  The response may be a reply, a **Busy**
   packet, an acknowledgement if the packet being sent is a reply, or an
   implicit piggy-backed acknowledgement.
1. Site failure is declared if silence is observed for a total period of time
   in the range $B_{total}$ to $2B_{total}$.

RPC2 does not try to accurately distinguish between site failure and
communication failure:  one may masquerade as the other, and  a single failure
`RPC2_DEAD` reflects both cases.  Loosely speaking, `N` characterises the
probability of packet loss in the communication medium, while `B-[total]`
characterises how sluggish a server may get before it is declared dead.

Given $B_{total}$ and $N$, we can determine $B_1, B_2, ..., B_N$ such that
$B_1 + B_2 + B_3 ... B_N = B_{total}$ and $B_i < B_{i+1}$.  Each $B_i$ is a
retry interval and the progressive lengthening of these intervals is to allow
for transient overloads at remote sites.  In RPC2, $B_{i+1} = 2B_{i}$.  In
practice we may place a minimum bound on the values for $B_i$s, to avoid
sending out packets too close to each other.

The RPC2 packet transmission algorithm is based on these concepts and is
outlined as follows:

```c
while(TRUE) {
    for (i = 0; i < N; i++) {
        send(packet);
        awaitresponse(B[i]);

        if (reply or lastack arrived)
            quit;

        if (BUSY arrived)
            break;
    }
    if (i >= N)
        goto TimeOut;

    sleep(B[total]);
}

TimeOut:
    mark connection RPC2_DEAD;
    mark dead all other connections to this (host, port) pair;
```

Failure is detected in time $B_{total}$ if the remote site dies just after the
`sleep()` call ends.  If the failure occurs immediately after the remote site
sends a **Busy** packet, failure is detected after $2B_{total}$.  These cases
bound the time it takes to detect failure.  Failure is also declared if all $N$
of the retries are lost due to communication failure.  This will occur in a
time exactly equal to $B_{total}$.

How does this mesh with side effects? The above algorithm will work regardless
of the duration of a side effect as long as there is some response from the
server in RPC (a **Busy packet**) or the side effect (any packet) at intervals
of $B_{total}$.  Note that it is immaterial whether the side effect involves
asynchronous Unix processes or not. If such processes are involved their
failure will be detected (perhaps as `RPC2_DEAD` failures or in other ways) and
reported by the remote server explicitly as `RPC2_SEFAIL2`.  Only if the remote
server is itself dead or unreachable is the RPC return code `RPC2_DEAD` and
this will occur no later than $2B_{total}$ after the failure.  In SFTP or where
TCP or other protocols are being used for side effects, the failure detection
mechanisms of these protocols will be relied upon to detect side effect
failure.

The tables at the end of this chapter show how the $N$ retransmissions take
place within $B_{total}$, for typical values of $N$ and $B_{total}$.  The
original attempt is at time 0.  The numbers in parentheses indicate the time
($B_N$) that RPC2 waits after the transmission of the last retry, before
declaring failure.  A lower limit of 300 milliseconds for the retry interval is
assumed.

### Adjusting Retransmission Intervals

Retransmission intervals in RPC are maintained on a connection-specific basis.
They are initialized with the values in the tables, given the number of retries
and total timeout specified in `RPC2_Init`.

There is a minimum bound of 300 milliseconds for the $B_i$ so that packets are
not sent out too close to each other.  For slower networks, this may still be
too short an interval.  The minimum interval should be at least a packet round
trip time (RTT).  To estimate the minimum interval, an RPC client collects RTT
observations on bind1-bind2, bind3-bind4, initmulticast-reply, and
request-reply pairs.  A "smoothed RTT" is calculated (a la TCP) and used to set
the lower limit on RPC retransmission intervals.  Whenever the RTT estimate is
changed, the retransmission intervals are recalculated, always maintaining the
given timeout.  The maximum value for the lower limit is $b_0$, so the client
has time for at least one retry.

RTT observations are collected by timestamping packets as described for TCP in
[@rfc1323].  The client stamps an outgoing packet with the current time. The
server echoes that timestamp on the next packet back to that client. The client
computes the RTT observation by subtracting the echoed stamp from the current
time.

Two longwords in the packet header are used to carry timestamps[^1].  Time
units must be meaningful for the range 10Mbps - 1200bps. To keep the stamp
small, time is measured relative to connection creation. The resolution of the
stamp is 10 msec.  Though it may be simpler to maintain a finer granularity
unit (the maximum packet size is 2900 bytes, which over Ethernet takes 2.32 ms
to transmit), the clock resolution on most machines is 10-20 ms.  The 10 msec
unit is still finer than TCP, which uses 500 ms ticks.  A connection can stay
open over a year before the time "wraps around" using 10 msec units.

[^1]: RPC uses one long word for collecting RTT observations. It uses the other
      field to send the time for the bind sequence to the server.  The bind
      time is used to initialize side effects. SFTP uses both timestamp fields.
      This is described further in [SFTP Internals](sftp_internals.md).

The RTT and RTT variance are stored scaled, as in TCP, for greater precision
and ease of computation. We need a longword for each, unlike TCP (which uses a
short). If we used a short, and scaled by 8, we would have only 12 bits for
time (40.950 sec). This is roughly the RTT of a packet over a 1200 bps link: it
takes 19.33 sec to transmit a 2900 byte packet.

For compatibility, if a packet arrives with a 0 timestamp, it means the sender
of the packet does not maintain or use RTT estimates.  RPC simply skips the RTT
update, and the retransmission timeout stays at its original setting.

Timestamps should be set as late as possible and collected as early as
possible.  Packets leaving the client are stamped in `rpc2_SendReliably` (for
multicast, in `mrpc_SendPacketReliably`).  Retries are restamped.  The RTT is
updated and retransmission intervals adjusted from the socket listener, upon
receipt of a bind2, bind4, or response.

Packets arriving at the server are checked in the `SocketListener`.  If the
request is a "good" init1, init3, or request, the timestamp is stashed in the
connection entry and the request time is taken.  The reply is stamped with the
original timestamp plus the service time.

Requests that provoke busies are stamped with the incoming timestamp -- the
service time is assumed to be 0. The client gets additional observations this
way.  Erroneous requests and binds that will be rejected are sent back with
null timestamps.  In such cases, there is usually no connection state to
update.

RPC does not currently take request and reply length into account in its RTT
estimate.  Because of this, the RTT has high variance at low bandwidths.
Ideally, RPC would derive an RTT independent of length, then apply it given the
length of the request and probable length of the reply.

See also [Retry Tables](retry_tables.md).
