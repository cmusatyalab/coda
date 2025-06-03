# Implementation of MultiRPC

Support for MultiRPC exists both at the language level and at the runtime
level.  The runtime level support includes the MultiRPC routines themselves
along with the associated library routines which perform argument packing and
unpacking.  The language level support consists mainly of the argument
descriptor information supplied by RP2Gen for each subsystem.  The client may
choose to interface directly with the runtime MultiRPC system without taking
advantage of the RP2Gen simplifications, but the discussion in the following
sections assumes the existence of the RP2Gen interface except where explicitly
noted otherwise.

The procedure for making a MultiRPC call is very similar to that for making an
RPC2 call. The subsystem is designed and the specification is written into a
*&lt;subsys&gt;.rpc2* file (the specification format is described in [RP2Gen
Stub Generator](rp2gen_user.md)). RP2Gen is then invoked on the specification
file, and it generates both the standard server and client side interfaces as
well as the MultiRPC argument descriptor structures and definitions for each
server operation. The relevant descriptor pointers are made available to the
client through the associated *&lt;subsys&gt;.h* file.

Once the interface has been specified, the subsystem implementor is responsible
for writing the server main loop and the procedures to perform the server
operations. This implementation is completely independent of any considerations
relating to MultiRPC; MultiRPC is completely transparent to the server side of
a subsystem.

From the clients perspective, making a MultiRPC call is slightly different from
the RPC2 case.  Instead of the procedure-like client side interface supplied by
the stub routines, the single library routine `MRPC_MakeMulti` is used to
interface to `RPC2_MultiRPC`. The use of the library routine represents a large
space savings in the executable files, but requires some additional information
from the client making the call (see [MRPC_MakeMulti](#mrpc_makemulti) and
[RPC2_MultiRPC](#rpc2_multirpc)).  The client is also responsible for supplying
a handler routine for any server operation which is used in a MultiRPC call.
This handler routine is called by RPC2 as each individual server response
arrives; it is used both for providing individual server return codes to the
client and for giving the client control over the continuation or termination
of the MultiRPC call. The handler routine is discussed in greater detail in the
following section, and its interface is described in
[HandleResult](#handleresult).

## The Client Handler

The client handler routine is intended to give the client control and
flexibility in handling the incoming server responses from the MultiRPC call.
For each connection specified in a `RPC2_MultiRPC` call, the client handler is
called either when a connection error is detected or when the server response
for that connection arrives. This allows the client to examine the replies as
they arrive, and provides the opportunity to perform incremental bookeeping and
analysis of the responses. The handler also has the ability to abort the
MultiRPC call at any time. A more detailed discussion of the handler
specifications can be found in [HandleResult](#handleresult).

Since a MultiRPC call could potentially last a long time, it is crucial to
provide the client with some measure of control over the progress and
termination of the call. With many server responses, there are many variables
that the client might wish to monitor in order to evaluate the progress of the
call.  In particular, the server responses and return codes themselves have a
significant effect on the clients perception of the progress of the call.  To
address these requirements, RPC2 periodically passes control to the client
during execution of the MultiRPC call. A client supplied routine designed to be
called as each server response arrives provides access to complete current
information about the status of the call; it also gives the client the ability
to perform any incremental processing he considers necessary or useful. The
client then indicates his decision to either continue accepting server
responses or to terminate the MultiRPC call via the handler return code.

The value of client control over the progress of the MultiRPC call can best be
illustrated with some specific examples.  One example is in the case of
connection errors. If the client requires responses on all of the designated
connections and one of them returns an error, then the final result of the
MultiRPC call will be useless and the remainder of the processing time will
have been wasted. With the client handler routine the client has the ability to
notice the connection error.  He then has the ability to abort the call, or
even to use the handler routine as an opportunity to rebind to the failed site
and make an RPC2 call on that connection.

Another example is in the implementation of a replicated server. A useful way
to deal with operation quorums (specified as some subset $N$ of the total number
of replicated servers) is to send messages out to all or many of the available
servers and abort the call as soon as the first $N$ responses  arrive. This has
the advantage of supplying the fastest possible execution for the replicated
call; furthermore, since the $N$ members of the quorum need not be chosen
explicitly, the call will rarely have to be repeated if one of the servers is
busy or inoperational.

The handler receives full sets of arguments each time it is called, along with
an index identifying the current connection. The types of the server arguments
to the client handler are identical to the types in the original MakeMulti
call: the argument list is in fact passed through RPC2 and returned to the
handler.  Any processing is permissible in the handler routine, although it
should be noted that since `RPC2_MultiRPC` does not support enqueueing of
server requests any call made on a connection already active in a MultiRPC call
will generate a return code of *RPC2_BUSY*.  Also, for lengthy blocking
computations the same cautions with respect to lightweight processes apply as
for RPC2.

It should also be noted that the use of the abort facility of the client handler
carries with it some risks. These are discussed in more detail in [Error Cases
and Abnormal Behavior](#error-cases-and-abnormal-behavior).

## Flow of Control in MultiRPC

The flow of control in MultiRPC is much the same as for RPC2 except for the
iterative calling of the client handler. The client initiates the MultiRPC call
by calling the library routine `MRPC_MakeMulti`. MakeMulti packs the client
arguments into a request buffer, and calls `RPC2_MultiRPC` with the request
buffer, some argument packing information, and a pointer to `MRPC_UnpackMulti`,
the library unpacking routine.

`RPC2_MultiRPC` sets up the processing environment, initializes the request
packet headers for all the designated servers, and performs any necessary side
effect initialization. It then calls an internal routine to perform the
transmission of the request packets. This transmission routine does not return
until either the client supplied timeout expires or until it has received
responses from all of the designated servers. Once the request packets have
been transmitted, the routine settles into a loop waiting for server responses
to arrive. As each response arrives, some preliminary processing is performed,
and any remaining side effect processing is completed.

Then RPC2 calls `MRPC_UnpackMulti` to unpack the response buffer into the
clients original arguments. `MRPC_UnpackMulti` unpacks the buffer and calls the
client handler routine with the current serverss information. The client then
performs whatever processing he wishes, and returns with his instructions to
continue or terminate the call. If he wishes to continue, the internal loop
continues until all the server responses have been received. Otherwise, the
loop terminates and the transmission routine cleans up any loose ends caused by
the termination.

Control then returns to `RPC2_MultiRPC`, which checks the return code and
returns to `MRPC_MakeMulti`. MakeMulti simply passes the supplied return code
back to the client as it returns.

Since side effects are completely determined by the **SE_Descriptor** and the
connection, extending the side effect mechanism to MultiRPC requires nothing
more than supplying a unique **SE_Descriptor** for each connection.

## MultiRPC Related Calls

### RPC2\_MultiRPC

`RPC2_MultiRPC` is the RPC2 runtime routine responsible for setting up the
internal state properly for sending the request packets to the specified
servers. It is called via the RPC2 library routine `MRPC_MakeMulti`.  One of
the arguments to MultiRPC is the ArgInfo structure. This structure is never
examined by RPC2, but is simply passed through UnpackMulti.  If the RP2Gen
interface is used, this argument is supplied by `MRPC_MakeMulti` and need not
concern the client. If the RP2Gen interface is not used, this can point to any
structure needed by the clients unpacking routine.

The UnpackMulti argument is also related to the RP2Gen interface. If the
RP2Gen interface is used, this argument is automatically supplied by
`MRPC_MakeMulti` and will point to the RPC2 library unpacking routine. If the
RP2Gen interface is not used, the client is responsible for supplying a
pointer to a routine matching the UnpackMulti specification (see
[MRPC_UnpackMulti](multi_user.md#mrpc_unpackmulti)).

### MRPC\_MakeMulti

`MRPC_MakeMulti` is the library routine which provides the parameter packing
interface to `RPC2_MultiRPC`. It takes the place of the individual client side
stub routines generated by RP2Gen.

In additon to the usual information supplied in an RPC2 call, it takes as
arguments RP2Gen generated argument and operation descriptors, the number of
servers to be called, and a pointer to a client supplied handler routine (see
[MRPC_MakeMulti](multi_user.md#mrpc_makemulti) for more detailed information).
Using the argument descriptors, `MRPC_MakeMulti` packs the supplied server
arguments into an RPC2 request buffer and creates a data structure containing
call specific information and a pointer to the client handler routine. It then
makes the MultiRPC call, and passes the final return code back to the client
when the call terminates.

OUT and INOUT parameters must be supplied in the form of arrays of pointers
to  the appropriate argument types. The parameter interface specifications
are discussed in [MultiRPC - C Interface
Specification](multi_user.md#c-interface-specification). The size of the array
is dependent on the number of servers designated by the client. For INOUT
parameters it is only necessary to actually fill in a value for the first
element of the array, although storage must be properly allocated for all of
the elements.

### MRPC\_UnpackMulti

`MRPC_UnpackMulti` is a RPC2 library routine which functions as the other half
of `MRPC_MakeMulti`.  It unpacks the contents of the response buffer into their
appropriate places in the clients arguments, and calls the client handler
routine. It returns with the return code supplied by the client handler
routine. If the RP2Gen interface is not used, the client must supply a pointer
to a routine with the specified interface (see
[MRPC_UnpackMulti](multi_user.md#mrpc_unpackmulti)).

### HandleResult

HandleResult is a place holder used to refer to the client-supplied handler
routine. It is called once for each connection by `MRPC_UnpackMulti` with the
newly arrived server reply. It can perform as much or as little processing as
the client deems necessary, and controls the continuation or termination of the
MultiRPC call with its return code. The argument specifications of this routine
are explained in detail in [HandleResult](multi_user.md#handleresult).

## Error Cases and Abnormal Behavior

The semantics for errors in the MultiRPC case are somewhat different from those
in the RPC2 case. Since several messages are being transmitted in the same
call, an error on one connection should not necessarily cause the call to
terminate. The client does, however, need to be informed of error states on any
of his connections. The handler routine will be called at most once for each
connection submitted to the MultiRPC call, either with an error condition or
with the server response. No packet will actually be sent on any connection for
which an error was detected in the course of processing.

As mentioned earlier, the additional flexibility provided by the client handler
routine incurs some risks. RPC2 makes no guarantees as to the state of the
connections which are not examined because of an abort by the client.  When the
client returns an abort code, there may still be some outstanding server
replies.  `RPC2_MultiRPC` increments the connection sequence number and resets
the connection state, thus pretending that the response in question was
actually received. This allows the system to continue with normal operation.

The risks of this approach can be illustrated with some examples. A client
makes a MultiRPC request **R1** to 3 servers, and terminates the call after two
of the server responses have been received. At server **S3**, the request has
been queued because the server was busy with a previous request. The client
then decides to make another MultiRPC request **R2** on a set of servers that
includes server **S3** from the first call. **S3** then receives **R2**, tagged
with the next logical sequence number,  on the same connection as **R1**.  If
**S3** has not yet begun processing **R1**, then  it will throw **R2** away
because it recognizes that its sequence number is too high. **S3** will then
proceed to process **R1** and send the response back to the  client; the
client, however, will promptly throw the response away as a retry because the
semantics of his abort command was to pretend that the response to **R1** from
**S3** had already arrived.

Now, assuming that the client chooses to terminate his second call before
 **S3** returns, the client and **S3** are completely out of synch. **S3**,
having thrown away **R2**, will always be expecting a packet with **R2**s
sequence number; the client, however, has already incremented the connection
at the termination of **R2**. In order to keep the connection from  hanging
around uselessly, **S3** will send a *RPC2_NAK* return code if it ever
receives a request **R3** on the same connection with a sequence number
greater than **R2**. This will kill the connection, forcing the client to
rebind if he wants to continue communicating with **S3**.

Another risk associated with the use of abort is the risk of not identifying
dead connections. If a server **S2** is dead but the client always chooses
to abort his MultiRPC call before a response from **S2** arrives, RPC2 may
not have time to notice that the connection is dead.

These problems are a result of the clients ability to ignore the responses on
some  connections in a MultiRPC call, and will generally only manifest
themselves in a case where a server is forced to queue a request because it is
busy processing an earlier request. This means that the MultiRPC call should be
used with caution in cases where simultaneous binding to a single site might
result, although the severity of the problem can be lessened by providing a
greater number of LWPs at the single site. It is important to note that these
problems arise only in the case where the client chooses to abort the call
before all replies have been received. However, the explicit NAK by the server
at least gives the client the opportunity to learn that something has gone
wrong with the connection and act accordingly.
