# RPC2 Runtime System

The purpose of this section is to describe the calls provided by the RPC2
library.  These calls deal with contiguous _Packet Buffers_ each of which
consists of a:

_Prefix_

:   which is of fixed length, and is used internally by the runtime system.  It
    is NOT transmitted.

_Header_

:   which is also of fixed length, and whose format is understood by the
    runtime system. The _opcode_ associated with the RPC, sequencing
    information, and the completion code returned by the remote site are the
    kinds of information found here.

_Body_

:   of arbitrary size.  It is NOT interpreted by the runtime system, and is
    used to transmit the input and output parameters of an RPC.

[`<rpc2/rpc2.h>`][rpc2_h]:octicons-link-external-16: is the actual header file
that defines the packet format as well as numerous other definitions used in
the call descriptions that follow.

[rpc2_h]: https://github.com/cmusatyalab/coda/blob/master/lib-src/rpc2/include/rpc2/rpc2.h

The following sections define the RPC2 runtime calls.  Some of these calls are
not relevant to you if you use RP2Gen.  Others, such as the initialization and
export calls, are pertinent to all users of RPC2.

## Client-related Calls

### RPC2\_NewBinding

``` c
int RPC2_NewBinding(
    in RPC2_HostIdent *Host,
    in RPC2_PortIdent *Port,
    in RPC2_SubsysIdent *Subsys,
    in RPC2_BindParms *Bparms,
    out RPC2_Handle *ConnHandle
);
```

Creates a new connection and binds to a remote server on a remote host.  The
subsystem information is passed on to that server to alert it to the kind of
remote procedure calls that it may expect on this connection.

A client-server version check is performed to ensure that the runtime systems
are compatible.  Note that there are really two version checks. One is for the
RPC network protocol and packet formats, and this must succeed.  The other
check reports a warning if you have a different RPC runtime system from the
server.  You may also wish to do a higher-level check, to ensure that the
client and server application code are compatible.

The SecurityLevel parameter determines the degree to which you can trust this
connection.  If `RPC2_OPENKIMONO` is specified, the connection is not
authenticated and no encryption is done on future requests and responses.  If
`RPC2_ONLYAUTHENTICATE` is specified, an authentication handshake is done to
ensure that the client and the server are who they claim to  to be (the fact
that the server can find SharedSecret from ClientIdent is assumed to be proof
of its identity.  If `RPC2_SECURE` is specified, the connection is
authenticated and all future transmissions on it are encrypted using a session
key generated during the authentication handshake.  `RPC2_HEADERSONLY` is
similar to `RPC2_SECURE`, except that only RPC headers are encrypted.

The kind of encryption used is specified in EncryptionType.  The remote site
must specify an `RPC2_GetRequest` with an EncryptionTypeMask that includes this
encryption type.

**Parameters**

:   `Host`

    :   The identity of the remote host on which the server to be contacted is
        located.  This may be specified as a string name or as an Internet
        address.  In the former case the RPC runtime system will do the
        necessary name resolution.

:   `Port`

    :   An identification of the server process to be contacted at the remote
        site. Ports are unique on a given host.  A port  may be specified as a
        string name or as an Internet port value.  In the former case the RPC
        runtime system will do the necessary name to port number conversion.
        Support for other kinds of ports (such as Unix domain) may be available
        in future.

:   `Subsys`

    :   Which of the potentially many subsystems supported by the remote server
        is desired.  May be specified as a number or as a name.  In the latter
        case, the RPC runtime system will do the translation from name to
        number.

:   `Bparms`

    :   Type RPC2\_BindParms is a struct containing 5 fields. The first field
        SecurityLevel is one of the constants `RPC2_OPENKIMONO`,
        `RPC2_ONLYAUTHENTICATE`, `RPC2_HEADERSONLY`, or `RPC2_SECURE`. The
        second field, EncryptionType, describes what kind of encryption to be
        used on this connection.  For example, `RPC2_XOR`, `RPC2_DES`, etc. is
        ignored if, SecurityLevel, is `RPC2_OPENKIMONO`.  The bind will fail if
        the remote site does not support the requested type of encryption. The
        third field, SharedSecret, is an encryption key known by the callback
        procedure on the server side to be uniquely associated with,
        ClientIdent. Used by the RPC runtime system in the authentication
        handshakes.  May be NULL if, SecurityLevel, is `RPC2_OPENKIMONO`. The
        fourth field, ClientIdent, provides the information for the server to
        uniquely identify this client and to obtain, SharedKey. Not interpreted
        by the RPC runtime system. Only the GetKeys callback procedure on the
        server side need understand the format of ClientIdent.  May be NULL if
        SecurityLevel is `RPC2_OPENKIMONO`. The fifth field, SideEffectType,
        describes what kind of side effects are to be associated with this
        connection.  The only side effects intially supported are
        bulk-transfers of files, identified by type, SMARTFTP. May be 0 if no
        side effects are ever to be attempted on this connection.

:   `ConnHandle`

    :   An unique integer returned by the call, identifying this connection.
        This is not necessarily a small-valued integer.

**Completion Codes**

:   `RPC2_SUCCESS`

    :   All went well.

:   `RPC2_NOBINDING`

    :   The specified host, server or subsystem could not be contacted.

:   `RPC2_WRONGVERSION`

    :   The client and server runtime systems are  incompatible.  Note that
        extreme incompatibilty may result in the server being unable to respond
        even with this error code.  In such a case, the server will appear to
        be down, resulting in an `RPC_NOBINDING` return code.

:   `RPC2_OLDVERSION`

    :   This is a warning.  The, RPC2_VERSION, values on client and server
        sides are different.  Normal operation is still possible, but one of
        you is running an obsolete version of the runtime system.  You should
        obtain the latest copy of the RPC runtime system and recompile your
        code.

:   `RPC2_NOTAUTHENTICATED`

    :   A SecurityLevel other than, RPC2_OPENKIMONO, was specified, and the
        server did not accept your credentials.

:   `RPC2_SEFAIL1`

    :   The associated side effect routine indicated a minor failure.  The
        connection is established, and usable.

:   `RPC2_SEFAIL2`

    :   The associated side effect routine indicated a serious failure.  The
        connection is not established.

:   `RPC2_FAIL`

    :   Some other mishap occurred.

### RPC2\_Bind

``` c
int RPC2_Bind(
    in long SecurityLevel,
    in long EncryptionType,
    in RPC2_HostIdent *Host,
    in RPC2_PortIdent *Port,
    in RPC2_SubsysIdent *Subsys,
    in long SideEffectType,
    in RPC2_CountedBS *ClientIdent,
    in RPC2_EncryptionKey *SharedSecret,
    out RPC2_Handle *ConnHandle
);
```

Obsolete: use [RPC2\_NewBinding](#rpc2_newbinding).

### RPC2\_MakeRPC

``` c
int RPC2_MakeRPC(
    in RPC2_Handle ConnHandle,
    in RPC2_PacketBuffer *Request,
    in Descriptor *SDesc,
    out RPC2_PacketBuffer **Reply,
    in struct timeval *Patience,
    in long EnqueueRequest
);
```

Make a remote procedure call (with possible side-effect).  The workhorse
routine, used to make remote calls after establishing a connection.  The call
is sequential and the calling lwp is blocked until the call completes.  The
associated side effect, if any, is finished before the call completes.  The
listed completion codes are from the local RPC stub.  Check the
`RPC2_ReturnCode` fields of the reply  and the status fields of SDesc to see
what the remote site thought of your request.  Without an explicit timeout
interval the remote site can take as long as it wishes to perform the requested
operation and associated side effects.  The RPC protocol checks periodically to
ensure that the remote site is alive.  If an explicit  Patience timeout
interval is specified,  the call must complete within that time.

**Parameters**

:   `ConnHandle`

    :   Identifies the connection on which the call is to be made.

:   `Request`

    :   A properly formatted request buffer. <!-- The `RPC2_PacketBuffer`
        structure is described in [RPC2\_PacketBuffer](#rpc2_packetbuffer). -->

:   `SDesc`

    :   A side effect descriptor with local fields filled in.  May be NULL if
        no side effects will occur as a result of this call.

:   `Reply`

    :   On return, it will point to a response buffer holding the response from
        the server.  You should free this buffer when you are done with it.

:   `Patience`

    :   Maximum time to wait for remote site to respond.  A NULL pointer
        indicates infinite patience.

:   `EnqueueRequest`

    :   Specifies whether the caller should be blocked if `ConnHandle` is
        already servicing an RPC request from some other LWP thread.  If this
        variable is 1 the caller is blocked.  Otherwise a return code of
        `RPC2_CONNBUSY` is returned.

**Completion Codes**

:   `RPC2_SUCCESS`

    :   All went well.

:   `RPC2_NOCONNECTION`

    :   ConnHandle does not refer to a valid connection.

:   `RPC2_TIMEOUT`

    :   A response was not received soon enough. Occurs only if the Patience
        parameter was non-NULL.

:   `RPC2_SEFAIL1`

    :   The associated side effect resulted in a minor failure.  Future calls
        on this connection will still work.

:   `RPC2_SEFAIL2`

    :   The associated side effect resulted in a serious failure.  Future calls
        on this connection will fail.

:   `RPC2_DEAD`

    :   The remote site has been deemed dead or unreachable.  Note that this is
        orthogonal to an RPC2\_TIMEOUT return code.

:   `RPC2_NAKED`

    :   The remote site sent an explicit negative acknowledgement.  This can
        happen if that site thought you were dead, or if someone at that site
        unbound your connection.

:   `RPC2_CONNBUSY`

    :   EnqueueRequest specified 0 and ConnHandle is currently servicing a
        call. Try again later.

:   `RPC2_FAIL`

    :   Other assorted calamities, such as attempting to use a connection
        already declared broken.

### RPC2\_MultiRPC

``` c
int RPC2_MultiRPC(
    in int HowMany,
    in RPC2_Handle ConnHandleList[],
    out RPC2_Integer *RCList,
    in out RPC2_Multicast *MCast,
    in RPC2_PacketBuffer *Request,
    in Descriptor SDescList[],
    in long (*UnpackMulti)(),
    in out ARG_INFO *ArgInfo,
    in struct timeval *Patience
);
```

Make a collection of remote procedure calls.  Logically identical to iterating
through ConnHandleList and making `RPC2_MakeRPC` calls to each specified
connection using Request as the request block, but this call will be
considerably faster than explicit iteration. The calling lightweight process
blocks until either the client requests that the call abort or one of the
following is true about each of the connections specified in ConnHandleList: a
reply has been received, a hard error has been detected for that connection, or
the specified timeout has elapsed.

The ArgInfo structure exists to supply argument packing and unpacking
information in the case where RP2Gen is used. Since its value is not examined
by RPC2, it can contain any pointer that a non-RP2Gen generated client wishes
to supply.

Similarly, UnpackMulti will point to a specific unpacking routine in the RP2Gen
case. If the RP2Gen interface is not used, you should assume that the return
codes of the supplied routine must conform to the specifications in
[UnpackMultiInfo](multi_user.md#mrpc_unpackmulti).

Side effects are supported as in the standard RPC2 case except that the client
must supply a separate `SE_Descriptor` for each connection. <!-- The format for
the `SE_Descriptor` argument is described in [Interface](#interface). -->  It
will often be useful to supply connection specific information such as unique
file names in the `SE_Descriptor`.

**Parameters**

:   `HowMany`

    :   How many servers to contact.

:   `ConnHandleList[]`

    :   Array of length HowMany, containing the handles of the connections on
        which calls are to be made.

:   `Request`

    :   A properly formatted request buffer.

:   `RCList`

    :   Array of length HowMany, into which RPC2 will place return codes for
        each of the connections specified in ConnHandleList.  May be specified
        as NULL if return codes will not be examined.

:   `MCast`

    :   Pointer to multicast structure.  Set to NULL for now.

:   `SDescList[]`

    :   Array of length HowMany, containing side effect descriptors for each of
        the connections specified in ConnHandleList.

:   `UnpackMulti`

    :   Pointer to unpacking routine called by RPC2 when each server response
        as received. If RP2Gen is used, this will be supplied by
        `MRPC_MakeMulti`.  Otherwise, it must be supplied by the client.

:   `ArgInfo`

    :   A pointer to a structure containing argument information. This structure
        is not examined by RPC2; it is passed to UnpackMulti.

:   `Patience`

    :   A timeout value.  If NULL, the call will block until all servers have
        responded.  If non-NULL, the call will block until all servers have
        responded or the timeout period has elapsed.

**Completion Codes**

:   `RPC2_SUCCESS`

    :   All servers returned successfully, or all servers until
        client-initiated abort returned successfully. Individual server
        response information is supplied via UnpackMulti to the user handler
        routine supplied in the ArgInfo structure.

:   `RPC2_TIMEOUT`

    :   The user specified timeout expired before all the servers responded.

:   `RPC2_FAIL`

    :   Something other than SUCCESS or TIMEOUT occurred. More detailed
        information is supplied via UnpackMulti to the user handler routine
        supplied in the ArgInfo structure.

## Server-related RPC Calls

### RPC2\_Export

``` c
int RPC2_Export(
    in RPC2_SubsysIdent *Subsys
);
```

Indicate willingness to accept calls for a subsystem Sets up internal tables so
that when a remote client performs an `RPC2_Bind()` operation specifying this
host-portal-subsystem triple, the RPC runtime system will accept it. A server
may declare itself to be serving more than one subsystem by making more than
one `RPC2_Export` calls.

**Parameters**

:   `Subsys`

    :   Specifies a subsystem that will be henceforth recognized by this
        server.  This is either an integer or a symbolic name that can be
        translated to the unique integer identifying this subsystem.

**Completion Codes**

:   `RPC2_SUCCESS`

    :   All went well.

:   `RPC2_DUPLICATESERVER`

    :   Your have already exported Subsys.

:   `RPC2_BADSERVER`

    :   Subsys is invalid.

:   `RPC2_FAIL`

    :   Something else went wrong.

### RPC2\_DeExport

``` c
int RPC2_DeExport(
    in RPC2_SubsysIdent *Subsys
);
```

Stop accepting new connections for one or all subsystems.  After this call, no
new connections for subsystem Subsys will be accepted.  The subsystem may,
however, be exported again at a later time.  Note that existing connections are
not broken by this call.

**Parameters**

:   `Subsys`

    :   Specifies the subsystem  to be deexported. This is either an integer or
        a symbolic name that can be translated to the unique integer
        identifying this subsystem. A value of NULL deexports all subsystems.

**Completion Codes**

:   `RPC2_SUCCESS`

    :   All went well.

:   `RPC2_BADSERVER`

    :   Subsys is not a valid subsystem, or has not been previously exported.

:   `RPC2_FAIL`

    :   Something else went wrong.

### RPC2\_GetRequest

``` c
int RPC2_GetRequest(
    in RPC2_RequestFilter *Filter,
    out RPC2_Handle *ConnHandle,
    out RPC2_PacketBuffer **Request,
    in struct timeval *Patience,
    in long (*GetKeys)(),
    in long EncryptionTypeMask,
    in long (*AuthFail)()
);
```

Wait for an RPC request or a new connection.  The call blocks the calling
lightweight process until a request is available, a new connection is made, or
until the specified timeout period has elapsed.  The Filter parameter allows a
great deal of flexibility in selecting precisely which calls are acceptable.
New connections result in a fake request with a body of type
`RPC2_NewConnection`. Do not try to do a `RPC2_SendResponse` to this call.  All
other `RPC2_GetRequest` calls should be eventually matched with a corresponding
`RPC2_SendResponse` call.

The fields of `RPC2_NewConnection` are self-explanatory.  Note that you must
invoke `RPC2_Enable()` after you have handled the new connection packet for
further requests to be visible.  If you are using RP2Gen, this is done for you
automatically by the generated code that deals with new connections.

The callback procedure for key lookup should be defined as follows:

``` c
long GetKeys(
    in RPC2_CoundedBS *ClientIdent,
    out RPC2_EncryptionKey *IdentKey,
    out RPC2_EncryptionKey *SessionKey
);
```

`GetKeys()` will be called at some point in the authentication handshake.  It
should return 0 if ClientIdent is successfully looked up, and -1 if the
handshake is to be terminated.  It should fill IdentKey with the key to be used
in the handshake, and SessionKey with an arbitrary key to be used for the
duration of this connection.  You may, of course, make SessionKey the same as
IdentKey.

The callback procedure for noting authentication failure should be defined as
follows:

``` c
long AuthFail(
    in RPC2_CoundedBS *ClientIdent,
    in RPC2_Integer EncryType,
    in RPC2_HostIdent *PeerHost,
    in RPC2_PortIdent *PeerPort
);
```

`AuthFail()` will be called after an `RPC2_NOTAUTHENTICATED` packet has been
sent to the client. The parameters give information about the client who was
trying to authenticate himself, the type of encryption requested, and the site
from which the `RPC2_Bind()` was attempted.  The callback procedure will
typically record this in a log file somewhere.

**Parameters**

:   `Filter`

    :   A filter specifying which requests are acceptable.  See description
        below.

:   `ConnHandle`

    :   Specifies the connection on which the request was received.

:   `Request`

    :   Value ignored on entry. On return, it will point to a buffer holding
        the response from the client.  Free this buffer after you are done with
        it.

:   `Patience`

    :   A timeout interval specifying how long to wait for a request.  If NULL,
        infinite patience is assumed.

:   `GetKeys`

    :   Pointer to a callback procedure to obtain authentication and session
        keys. See description below. May be NULL if no secure bindings to this
        server are to be accepted.

:   `EncryptionTypeMask`

    :   A bit mask specifying which types of encryption is supported. Binds
        from clients who request an encryption type not specified in this mask
        will be rejected.

:   `AuthFail`

    :   Pointer to a callback procedure to be called when an authentication
        failure occurs. See description below.  May be NULL if server does not
        care to note such failures.

**Completion Codes**

:   `RPC2_SUCCESS`

    :   All went well.

:   `RPC2_TIMEOUT`

    :   Specified time interval expired.

:   `RPC2_BADFILTER`

    :   A nonexistent connection or subsystem was specified in Filter.

:   `RPC2_SEFAIL1`

    :   The associated side effect routine indicated a minor failure.  Future
        calls on this connection will still work.

:   `RPC2_SEFAIL2`

    :   The associated side effect routine indicated a serious failure.  Future
        calls on this connection will fail too.

:   `RPC2_DEAD`

    :   You were waiting for requests on a specific connection and that site
        has been deemed dead or unreachable.

:   `RPC2_FAIL`

    :   Something irrecoverable happened.

### RPC2\_Enable

``` c
int RPC2_Enable(
    in RPC2_Handle ConnHandle
);
```

Allow servicing of requests on a new connection.  Typically invoked  by the
user at the end of his NewConnection routine, after setting up his higher-level
data structures appropriately.  Until a connection is enabled, RPC2 guarantees
that no requests on that connection will be returned in a `RPC2_GetRequest`
call.  Such a request from a client will, however, be held and responded to
with `RPC2_BUSY` signals until the connection is enabled.  This call is present
primarily to avoid race hazards in higher-level connection establishment.  Note
that RP2Gen automatically generates this call after a NewConnection routine.

**Parameters**

:   `ConnHandle`

    :   Which connection is to be enabled.

**Completion Codes**

:   `RPC2_SUCCESS`

    :   Enabled the connection.

:   `RPC2_NOCONNECTION`

    :   A bogus connection was specified.

### RPC2\_SendResponse

``` c
int RPC2_SendResponse(
    in RPC2_Handle ConnHandle,
    in RPC2_PacketBuffer *Reply
);
```

Respond to a request from my client.  Sends the specified reply to the caller.
Any outstanding side effects are completed before Reply is sent.  Encryption,
if any, is done in place and will clobber the Reply buffer.  Note that this
call returns immediately after sending the reply; it does not wait for an
acknowledgement from the client.

**Parameters**

:   `ConnHandle`

    :   Which connection the response is to be sent on.

:   `Reply`

    :   A filled-in buffer containing the reply to be sent to the client.

**Completion Codes**

:   `RPC2_SUCCESS`

    :   I sent your response.

:   `RPC2_NOTWORKER`

    :   You were not given a request to service.

:   `RPC2_SEFAIL1`

    :   The associated side effect routine indicated a minor failure.  Future
        calls on this connection will still work.

:   `RPC2_SEFAIL2`

    :   The associated side effect routine indicated a serious failure.  Future
        calls on this connection will fail too.

:   `RPC2_FAIL`

    :   Some irrecoverable failure happened.

### RPC2\_InitSideEffect

``` c
int RPC2_InitSideEffect(
    in RPC2_Handle ConnHandle,
    in SE_Descriptor *SDesc
);
```

Initiates the side effect specified by SDesc on ConnHandle.  The call does not
wait for the completion of the side effect.  If you need to know what happened
to the side effect, do a `RPC2_CheckSideEffect` call with appropriate flags.

**Parameters**

:   `ConnHandle`

    :   The connection on which the side effect is to be initiated.

:   `SDesc`

    :   A filled-in side effect descriptor.

**Completion Codes**

:   `RPC2_SUCCESS`

    :   The side effect has been initiated.

:   `RPC2_NOTSERVER`

    :   Only one side effect is allowed per RPC call. This has to be initiated
        between the GetRequest and SendResponse of that call.  You are
        violating one of these restrictions.

:   `RPC2_SEFAIL1`

    :   The associated side effect routine indicated a nonfatal failure.
        Future calls on this connection will work.

:   `RPC2_SEFAIL2`

    :   The associated side effect routine indicated a serious failure.  Future
        calls on this connection will fail too.

:   `RPC2_FAIL`

    :   Some irrecoverable failure happened.

### RPC2\_CheckSideEffect

``` c
int RPC2_CheckSideEffect(
    in RPC2_Handle ConnHandle,
    in out SE_Descriptor *SDesc,
    in long Flags
);
```

Checks the status of a previously initiated side effect.  This is a
(potentially) blocking call, depending on the specified flags.

**Parameters**

:   `ConnHandle`

    :   The connection on which the side effect has been initiated.

:   `SDesc`

    :   The side effect descriptor as it was returned by the previous
        RPC2_InitSideEffect call on ConnHandle.  On output, the status fields
        are filled in.

:   `Flags`

    :   Specifies what status is desired.  This call will block until the
        requested status is available.  This is a bit mask, with
        `RPC2_GETLOCALSTATUS` and `RPC2_GETREMOTESTATUS` bits indicating local
        and remote status.  A Flags value of 0 specifies a polling status
        check: no blocking will occur and the currently known local and remote
        status will be returned.

**Completion Codes**

:   `RPC2_SUCCESS`

    :   The requested status fields have been made available.

:   `RPC2_NOTSERVER`

    :   No side effect is ongoing on ConnHandle.

:   `RPC2_SEFAIL1`

    :   The associated side effect routine indicated a nonfatal failure.

:   `RPC2_SEFAIL2`

    :   The associated side effect routine indicated a serious failure.

:   `RPC2_FAIL`

    :   Some irrecoverable failure happened.

## Miscellaneous Calls

### RPC2\_Init

``` c
int RPC2_Init(
    in char *VersionId,
    in long Options,
    in RPC2_PortIdent *PortList[],
    in long HowManyPorts,
    in long RetryCount,
    in struct timeval *KeepAliveInterval
);
```

Initializes the RPC runtime system in this process.  This call should be made
before any other call in this package is made.  It should be preceded by an
initialization call to the LWP package and a call to `SE_SetDefaults` with
InitialValues as argument. If you get a wrong version indication, obtain a
consistent version of the header files and the RPC runtime library and
recompile your code. Note that this call incorporates a call to initialize
IOMGR. RetryCount and KeepAliveInterval together define what it means for a
remote site to be dead or unreachable.  Packets are retransmitted at most
RetryCount times until positive acknowledgement of their receipt is received.
This is usually piggy-packed with useful communication, such as the reply to a
request.  The KeepAliveInterval is used for two purposes: to determine how
often to check a remote site during a long RPC call, and to calculate the
intervals between the RetryCount retransmissions of a packet.

The RPC runtime system guarantees detection of remote site failure or network
partition within a time period in the range  KeepAliveInterval to twice
KeepAliveInterval.  See [Failure Detection](failure.md) for further information
on the retry algorithm.

Remember to activate each side effect that you are interested in by invoking
the corresponding _SE_\_Activate call, prior to calling `RPC2_Init`.

You may get a warning about `SO_GREEDY` being undefined, if your kernel does
not have an ITC bug fix.  RPC2 will still work but may be slower and more
likely to drop connections during bulk transfer.  This is because of
insufficient default packet buffer space  within the Unix kernel.

**Parameters**

:   `VersionId`

    :   Set this to the constant `RPC2_VERSION`.  The current value of this
        string constant must be identical to the value at the time the client
        runtime system was compiled.

:   `Options`

    :   Right now there are no options.

:   `PortList[]`

    :   An array of unique network addresses within this machine, on which
        requests can be listened for, and to which responses to outgoing calls
        can be made.  In the Internet domain this translates into a port number
        or a symbolic name that can be mapped to a port number.   You need to
        specify this parameter even if you are only going to be a client and
        not export any subsystems.  A value of NULL will cause RPC2 to select
        an arbitrary, nonassigned port.

:   `HowManyPorts`

    :   Specifies the number of elements in the array PortList.

:   `RetryCount`

    :   How many times to retransmit a packet before giving up all hope of
        receiving acknowledgement of its receipt.  Should be in the range 1 to
        30. Use a value of -1 to obtain the default.

:   `KeepAliveInterval`

    :   How often to probe a peer during a long RPC call.  This value is also
        used to calculate the retransmission intervals when packet loss is
        suspected by the RPC runtime system. Use NULL to obtain the default.

**Completion Codes**

:   `RPC2_SUCCESS`

    :   All went well.

:   `RPC2_WRONGVERSION`

    :   The client and server runtime systems are  incompatible.  Note that
        extreme incompatibilty may result in the server being unable to respond
        even with this error code.  In such a case, the server will appear to
        be down, resulting in an `RPC_NOBINDING` return code.

:   `RPC2_LWPNOTINIT`

    :   The Lightweight Process system has not been initialized.  This is done
        by calling `LWP_Init()`.

:   `RPC2_BADSERVER`

    :   The `PortList` field specifies an invalid address.

:   `RPC2_DUPLICATESERVER`

    :   An entry in PortList specifies an address which is already in use on
        this machine.

:   `RPC2_SEFAIL1`

    :   The associated side effect routine indicated a minor failure.

:   `RPC2_SEFAIL2`

    :   The associated side effect routine indicated a serious failure.

:   `RPC2_FAIL`

    :   Something else went wrong.

### RPC2\_Unbind

``` c
int RPC2_Unbind(
    in RPC2_Handle ConnHandle
);
```

Terminate a connection by client or server.  Removes the binding associated
with the specified connection.  Normally a higher-level  disconnection should
be done by an RPC just prior to this call.  Note that this call may be used
both by a server and a client, and that no client/server communication occurs:
the unbinding is unilateral.

**Parameters**

:   `ConnHandle`

    :   Identifies the connection to be terminated.

**Completion Codes**

:   `RPC2_SUCCESS`

    :   All went well.

:   `RPC2_NOCONNECTION`

    :   ConnHandle is bogus.

:   `RPC2_SEFAIL1`

    :   The associated side effect routine indicated a minor failure.

:   `RPC2_SEFAIL2`

    :   The associated side effect routine indicated a serious failure.

:   `RPC2_FAIL`

    :   Other assorted calamities.

### RPC2\_AllocBuffer

``` c
int RPC2_AllocBuffer(
    in long MinBodySize,
    out RPC2_PacketBuffer **Buff
);
```

Allocate a packet buffer.  Allocates a packet buffer of at least the requested
size.  The BodyLength field in the header of the allocated packet is set to
MinBodySize.  The RPC runtime system maintains its own free list of buffers.
Use this call in preference to `malloc()`.

**Parameters**

:   `MinBodySize`

    :   Minimum acceptable body size for the packet buffer.

:   `Buff`

    :   Pointer to the allocated buffer.

**Completion Codes**

:   `RPC2_SUCCESS`

    :   Buffer has been allocated and *Buff points to it.

:   `RPC2_FAIL`

    :   Could not allocate a buffer of requested size.

### RPC2\_FreeBuffer

``` c
int RPC2_FreeBuffer(
    in out RPC2_PacketBuffer **Buff
);
```

Free a packet buffer.  Returns a packet buffer to the internal free list.
Buff is set to NULL specifically to simplify locating bugs in buffer usage.

**Parameters**

:   `Buff`

    :   Pointer to the buffer to be freed. Set to NULL by the call.

**Completion Codes**

:   `RPC2_SUCCESS`

    :   Buffer has been freed. `*Buff` has been set to NULL.

:   `RPC2_FAIL`

    :   Could not free buffer.

### RPC2\_GetPrivatePointer

``` c
int RPC2_GetPrivatePointer(
    in RPC2_Handle WhichConn,
    out char **PrivatePtr
);
```

Obtain private data mapping for a connection.  Returns a pointer to the private
data associated with a connection.  No attempt is made to validate this
pointer.

**Parameters**

:   `WhichConn`

    :   Connection whose private data pointer is desired.

:   `PrivatePtr`

    :   Set to point to private data.

**Completion Codes**

:   `RPC2_SUCCESS`

    :   *PrivatePtr now points to the private data associated with this
        connection.

:   `RPC2_FAIL`

    :   Bogus connection specified.

### RPC2\_SetPrivatePointer

``` c
int RPC2_SetPrivatePointer(
    in RPC2_Handle WhichConn,
    in char *PrivatePtr
);
```

Set private data mapping for a connection.  Sets the private data pointer
associated with a connection.  No attempt is made to validate this pointer.

**Parameters**

:   `WhichConn`

    :   Connection whose private data pointer is to be set.

:   `PrivatePtr`

    :   Pointer to private data.

**Completion Codes**

:   `RPC2_SUCCESS`

    :   Private pointer set for this connection.

:   `RPC2_FAIL`

    :   Bogus connection specified.

### RPC2\_GetSEPointer

``` c
int RPC2_GetSEPointer(
    in RPC2_Handle WhichConn,
    out char **SEPtr
);
```

Obtain per-connection side-effect information.  Returns a pointer to the side
effect data associated with a connection.  No attempt is made to validate this
pointer.  This call is should only by the side effect routines, not by clients.

**Parameters**

:   `WhichConn`

    :   Connection whose side-effect data pointer is desired.

:   `SEPtr`

    :   Set to point to side effect data.

**Completions Codes**

:   `RPC2_SUCCESS`

    :   `*SEPtr` now points to the side effect data associated with this
        connection.

:   `RPC2_FAIL`

    :   Bogus connection specified.

### RPC2\_SetSEPointer

``` c
int RPC2_SetSEPointer(
    in RPC2_Handle WhichConn,
    in char *SEPtr
);
```

Set per-connection side-effect connection.  Sets the side effect data pointer
associated with a connection.  No attempt is made to validate this pointer.
This call should only be used by the side effect routines, not by clients.

**Parameters**

:   `WhichConn`

    :   Connection whose side effect pointer is to be set.

:   `SEPtr`

    :   Pointer to side effect data.

**Completion Codes**

:   `RPC2_SUCCESS`

    :   Side effect pointer set for this connection.

:   `RPC2_FAIL`

    :   Bogus connection specified.

### RPC2\_GetPeerInfo

``` c
int RPC2_GetPeerInfo(
    in RPC2_Handle WhichConn,
    out RPC2_PeerInfo *PeerInfo
);
```

Obtain miscellaneous connection information.  Returns the peer information for
a connection.  Also returns other miscellaneous connection-related information,
such as the securrity level in use. This information may be used by side-effect
routines or high-level server code to perform RPC bindings in the opposite
direction. The RemoteHandle and Uniquefier  information are  useful as
end-to-end identification between client code and server code.

**Parameters**

:   `WhichConn`

    :   Connection whose peer you wish to know about.

:   `PeerInfo`

    :   Data structure to be filled.

**Completion Codes**

:   `RPC2_SUCCESS`

    :   Peer information has been obtained for this connection.

:   `RPC2_FAIL`

    :   Bogus connection specified.

### RPC2\_LamportTime

Get Lamport time.  Returns the current Lamport time.  Bears no resemblance to
the actual time of day. Each call is guaranteed to return a value at least one
larger than the preceding call.  Every RPC packet sent and received by this
Unix process has a Lamport time field in its header.  The value returned by
this call is guaranteed to be  greater than any  Lamport time field received or
sent before now.  Useful for generating unique timestamps in a distributed
system.

### RPC2\_DumpState

``` c
int RPC2_DumpState(
    in FILE *OutFile,
    in long Verbosity
);
```

Dump internal RPC state.  You should typically call this routine after calling
[`RPC_DumpTrace`](#rpc2_dumptrace).

**Parameters**

:   `OutFile`

    :   File on which the dump is to be produced.  A value of NULL implies
        stdout.

:   `Verbosity`

    :   Controls the amount of information dumped.  Right now two values 0 and
        1 are meaningful.

**Completion Codes**

:   `RPC2_SUCCESS`

    :   The dump has been produced.

### RPC2\_InitTraceBuffer

``` c
int RPC2_InitTraceBuffer(
    in long HowMany
);
```

Set trace buffer size.  Allows you to create and change the trace buffer at
runtime.  All existing trace entries are lost.

**Parameters**

:   `HowMany`

    :   How many entries the trace buffer should have.  Set it to zero to
        delete trace buffer.

**Completion Codes**

:   `RPC2_SUCCESS`

    :   The trace buffer has been adjusted appropriately.

### RPC2\_DumpTrace

``` c
int RPC2_DumpTrace(
    in FILE *OutFile,
    in long HowMany
);
```

Print a trace of recent RPC calls and packets received.

!!! note
    It is not necessary for `RPC2_Trace` to be currently set.  You can collect
    a trace and defer calling `RPC2_DumpTrace` until a convenient time.  This
    call does not alter the current value of `RPC2_Trace`.

**Parameters**

:   `OutFile`

    :   File on which the trace is to be produced.  A value of NULL implies
        stdout.

:   `HowMany`

    :   The HowMany most recent trace entries are printed.  A value of 0
        implies as many trace entries as possible.  Values larger than
        `TraceBufferLength` specified in `RPC2_Init` are meaningless.

**Completion Codes**

:   `RPC2_SUCCESS`

    :   The requested trace has been produced.

:   `RPC2_FAIL`

    :   The trace buffer had no entries.

### RPC2\_SetLog

``` c
void RPC2_SetLog(
    in FILE *file,
    int level
);
```

Set the logfile and debug level to use.  The debug level defines the verbosity
of trace and log entries that will be written by a `RPC2_DumpTrace`.

**Parameters**

:   `file`

    :   File to send trace dumps and log entries to.

:   `level`

    :   Value to set the debug level to.

### SE\_SetDefaults, SFTP\_SetDefaults

``` c
int ???_SetDefaults(
    out ???_Initializer *Initializer
);
```

Set an SE initializer to its default values.  Each side effect type, _???_,
defines an initialization structure type, `???_Initializer`, and an
initialization routine, `???_SetDefaults()`.

A typical initialization sequence consists of the following: for each side
effect, _???_, that you care about,

- declare a local variable of type `???_Initializer`.
- call `???_SetDefaults()` with this local variable as argument.
- selectively modify those initial values you care about in the local variable,
  and
- call `???_Activate()` with this local variable as argument.

Finally call `RPC2_Init`.

This allows you to selectively set parameters of _???_ without having to know
the proper values for all of the possible parameters. Alas, if only C allowed
initialization in type declarations  this routine would be unnecessary.

**Parameters**

:   `Initializer`

    :   Initializer for side effect _???_ which you wish to set to default
        values.

**Completion Codes**

:   `RPC2_SUCCESS`

    :   Initialization Succeeded.

### SE\_Activate, SFTP\_Activate

``` c
int ???_Activate(
    in ???_Initializer *Initializer
);
```

Activates a side effect type and initializes it.  Activates side effect _???_.
Code corresponding to this side effect will not be linked in otherwise. See
comment for [`???_SetDefaults()`](#se_setdefaults-sftp_setdefaults) for further
details.

**Parameters**

:   `Initializer`

    :   Initializer for side effect _???_ which you wish to activate.

**Completion Codes**

:   `RPC2_SUCCESS`

    :   Activation Succeeded.

### RPC2\_ErrorMsg

``` c
char *RPC2_ErrorMsg(
    in long ReturnCode
);
```

Describe RPC2 error code.  Converts ReturnCode into a string that can be used
for printing error messages.  Note that this is the only RPC2 call that returns
a non-integer value.

**Parameters**

:   `ReturnCode`

    :   Error code returned by any RPC2 call.

### RPC2\_SetColor

``` c
int RPC2_SetColor(
    in RPC2_Handle *Conn,
    in RPC2_Integer NewColor
);
```

Set the color of a connection.  A color is an integer between 0 and 255 that is
associated with a connection.  When a packet is sent out on a connection, it
acquires this color.  Colors have no implicit significance to RPC2.  But they
can be used by other packages such as the Coda failure emulator package to
selectively induce failures.  For example, in debugging an implementation of a
two-phase commit protocol, one needs to test the situation where a failure
occurs between the two phases. This situation can be detected by the failure
emulator by using packets of different colors for the different phases.  A
change in color of a connection takes effect with the next packet that is sent
out.

**Parameters**

:   `Conn`

    :   Connection whose color is to be changed.

:   `NewColor`

    :   The lowest-order byte of this value is used as the new color for this
        connection.

**Completion Codes**

:   `RPC2_SUCCESS`

    :   The connection has been colored as requested.

:   `RPC2_NOCONNECTION`

    :   Connection specified is bogus.

### RPC2\_GetColor

``` c
int RPC2_GetColor(
    in RPC2_Handle *Conn,
    out RPC2_Integer *Color
);
```

Obtain current color of a connection.  Useful if a package using RPC2 wants to
save current color, set new color, then restore original color.

**Parameters**

:   `Conn`

    :   Connection whose color is to be obtained.

:   `Color`

    :   Current color of connection.

**Completion Codes**

:   `RPC2_SUCCESS`

    :   Color obtained.

:   `RPC2_NOCONNECTION`

    :   Connection specified is bogus.
