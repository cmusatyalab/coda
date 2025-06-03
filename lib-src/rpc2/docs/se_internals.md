# Adding New Kinds of Side Effects

This chapter is not intended for the average user.  Rather, it is meant for
a system programmer who intends to add support for a new kind of side effect.
To do this, the system programmer must do the following:

1. Define an appropriate side effect descriptor, add it to the header file se.h
   and to the discriminated union in the definition of `SE_Descriptor`.

1. Define an appropriate Initializer structure and a corresponding component in
   the `SE_Initializer` structure in file `se.h`.

1. Write a set of routines corresponding to each of the `SE_XXX` routines
   described in the following pages.  This includes a `SE_Activate()` routine
   to enlarge the table in file se.c,  and a `SE_SetDefaults()` routine to deal
   with `SE_Initializer` structures.

## Usage Notes

1. You will modify two RPC2 files (se.h and se.c), and add one more file
   containing the code implementing your new side effect.  Also modify the
   Makefile to compile and link in your new file.

1. Client and server programs will cause the appropriate side effect routines
   to be linked in by calling the appropriate `SE_Activate()` for each side
   effect they are interested in.  Note that these calls must precede
   `RPC_Init()`.

1. None of these procedures will be called for a connection, if the `RPC2_Bind`
   that created the connection specified NULL for the SideEffectType parameter.

1. In each of the calls, ConnHandle is the handle identifying the connection
   on which the side effect is desired.  It is not likely to be a small
   integer.  Since you cannot access the internal data structures of the RPC2
   runtime system, you cannot use this for much.  It is passed to you primarily
   for identification.

1. You can use `RPC2_GetSEPointer()` and `RPC2_SetSEPointer()` to associate
   per-connection side effect data structures.

1. Use `RPC2_GetPeerInfo()` to get the identity of a connections peer.

1. Three return codes: *RPC2_SUCCESS* and *RPC2_SEFAIL1* and *RPC2_SEFAIL2* are
   recognized for each of the calls.  The successful return causes the RPC
   runtime system to resume normal execution from the point at which the side
   effect routine was invoked. The failure returns abort the call at that point
   and returns *RPC2_SEFAIL1* or *RPC_SEFAIL2* to the client or server code
   that invoked the RPC system call.  *RPC2_SEFAIL1* is an error, but not a
   fatal error.  Future RPC calls on this connection will still work.
   *RPC2_SEFAIL2* is a fatal error.

## Entry Point Templates

### SE\_Init

``` c
int SE_Init(void);
```

Called just prior to return from `RPC2_Init`.

**Completion Codes:**

:   `RPC2_SUCCESS`
:   `RPC2_SEFAIL1`
:   `RPC2_SEFAIL2`

### SE\_Bind1

``` c
int SE_Bind1(
    in RPC2_Handle ConnHandle,
    in RPC2_CountedBS *ClientIdent
);
```

Called on `RPC2_Bind` on client side.  The call is made just prior to sending
the first connection-establishment packet to the server.  The connection
establishment is continued only if *RPC2_SUCCESS* is returned.

**Parameters**

:   `ConnHandle`

    :   Handle identifying the connection on which the side effect is desired.

:   `ClientIdent`

    :   The client identifier passed in the `RPC2_Bind` call.

**Completion Codes:**

:   `RPC2_SUCCESS`
:   `RPC2_SEFAIL1`
:   `RPC2_SEFAIL2`

### SE\_Bind2

``` c
int SE_Bind2(
    in RPC2_Handle ConnHandle,
    in struct timeval *BindTime
);
```

Called on `RPC2_Bind` on client side.  The call is made just after the
connection is successfully established, before control is returned to the
caller.  `BindTime` is the amount of time the bind took because of the network.
If `SE_Bind2` returns `RPC2_SEFAIL1` or `RPC2_SEFAIL2`, that code is returned
as the result of the `RPC2_Bind`.  Otherwise the usual code is returned.

**Parameters**

:   `ConnHandle`

    :   Handle identifying the connection on which the side effect is desired.

:   `BindTime`

    :   The amount of time the bind took because of the network.

**Completion Codes:**

:   `RPC2_SUCCESS`
:   `RPC2_SEFAIL1`
:   `RPC2_SEFAIL2`

### SE\_Unbind

``` c
int SE_Unbind(in RPC2_Handle ConnHandle);
```

Called when `RPC2_Unbind` is executed on the client or server side.  You are
expected to free any side effect storage you associated with this connection,
and to do whatever cleanup is necessary.  Note that the connection state is
available to you and is not destroyed until you return `RPC2_SUCCESS`.

**Parameters**

:   `ConnHandle`

    :   Handle identifying the connection on which the side effect is desired.

**Completion Codes:**

:   `RPC2_SUCCESS`
:   `RPC2_SEFAIL1`
:   `RPC2_SEFAIL2`

### SE\_NewConnection

``` c
int SE_NewConnection(
    in RPC2_Handle ConnHandle,
    in RPC2_CountedBS *ClientIdent
);
```

Called on `RPC2_NewConnection` on server side.  The call is made just after the
connection is successfully established, before control is returned to the caller.

**Parameters**

:   `ConnHandle`

    :   Handle identifying the connection on which the side effect is desired.

:   `ClientIdent`

    :   The client identifier passed in the `RPC2_Bind` call.

**Completion Codes:**

:   `RPC2_SUCCESS`
:   `RPC2_SEFAIL1`
:   `RPC2_SEFAIL2`

### SE\_MakeRPC2

``` c
int SE_MakeRPC2(
    in RPC2_Handle ConnHandle,
    in SE_Descriptor *SDesc,
    in RPC2_PacketBuffer *Reply
);
```

Called just after `Reply` has been received, after decryption and host ordering
of header fields.  Examine the SideEffectFlags and SideEffectDataOffset fields
to determine if there is piggy-backed side effect data for you in Reply.  If
you remove data, remember to update the BodyLength field of the header in
`Reply`.  SDesc points to the side effect descriptor.  You will probably wish
to fill in the status fields of this descriptor.  If the MakeRPC call fails for
some reason, this routine will be called with a `Reply` of NULL.  This allows
you to take suitable cleanup action.

**Parameters**

:   `ConnHandle`

    :   Handle identifying the connection on which the side effect is desired.

:   `SDesc`

    :   The side effect descriptor.

:   `Reply`

    :   The reply packet.

**Completion Codes**

:   `RPC2_SUCCESS`
:   `RPC2_SEFAIL1`
:   `RPC2_SEFAIL2`

### SE\_GetRequest

``` c
int SE_GetRequest(
    in RPC2_Handle ConnHandle,
    inout RPC2_PacketBuffer *Request
);
```

Called just prior to successful return of Request to the server.  You should
look at Request, extract side effect data if any, modify the header fields
appropriately.

Called after a request has been completely filled, just prior to network
ordering of header fields, encryption and transmission.  You may use the Prefix
information to determine the actual size of the buffer corresponding to
`*RequestPtr`.  If you add data, remember to update the BodyLength field of the
header in `*RequestPtr`.  You also probably wish to update the SideEffectFlags
and SideEffectDataOffset fields of the header.  SDesc points to the side effect
descriptor passed in by the client.

If you need more space than available in the buffer passed to you, you may
allocate a larger packet, copy the current contents and add additional data.
Return a pointer to the packet you allocated in RequestPtr: this is the packet
that will actually get sent over the wire.  DO NOT free the buffer pointed to
by RequestPtr initially.  If you allocate a packet, it will be freed
immediately after successful transmission.

**Parameters**

:   `ConnHandle`

    :   Handle identifying the connection on which the side effect is desired.

:   `Request`

    :   The request packet.

**Completion Codes**

:   `RPC2_SUCCESS`
:   `RPC2_SEFAIL1`
:   `RPC2_SEFAIL2`

### SE\_InitSideEffect

``` c
int SE_InitSideEffect(
    in RPC2_Handle ConnHandle,
    inout SE_Descriptor *SDesc
);
```

Called when the server does an `RPC2_InitSideEffect` call. You will probably
want to examine some fields of SDesc and fill in some status-related fields.
Note that there is no requirement that you should actually initiate any side
effect action. You may choose to piggy back the side effect with the reply
later.

**Parameters**

:   `ConnHandle`

    :   Handle identifying the connection on which the side effect is desired.

:   `SDesc`

    :   The side effect descriptor.

**Completion Codes**

:   `RPC2_SUCCESS`
:   `RPC2_SEFAIL1`
:   `RPC2_SEFAIL2`

### SE\_CheckSideEffect

``` c
int SE_CheckSideEffect(
    in RPC2_Handle ConnHandle,
    inout SE_Descriptor *SDesc,
    in long Flags
);
```

Called when the server does an `RPC2_CheckSideEffect` call.  The Flags
parameter will specify what status is desired.  You may have to actually
initiate the side effect, depending on the circumstances.

**Parameters**

:   `ConnHandle`

    :   Handle identifying the connection on which the side effect is desired.

:   `SDesc`

    :   The side effect descriptor.

:   `Flags`

    :   The flags.

**Completion Codes**

:   `RPC2_SUCCESS`
:   `RPC2_SEFAIL1`
:   `RPC2_SEFAIL2`

### SE\_SendResponse

``` c
int SE_SendResponse(
    in RPC2_Handle ConnHandle,
    inout RPC2_PacketBuffer **ReplyPtr
);
```

Called just before the reply packet is network-ordered, encrypted and
transmitted. You may wish to add piggy-back data to the reply; modify the
BodyLength field in that case.  If you are not piggybacking data, make sure
that the side effect is complete before returning from this call.

If you need more space than available in the buffer passed to you, you may
allocate a larger packet, copy the current contents and add additional data.
Return a pointer to the packet you allocated in ReplyPtr: this is the packet
that will actually get sent over the wire.  DO NOT free the buffer pointed to
by ReplyPtr initially.  If you allocate a packet, it will be freed immediately
after successful transmission.

**Parameters**

:   `ConnHandle`

    :   Handle identifying the connection on which the side effect is desired.

:   `ReplyPtr`

    :   The reply packet.

**Completion Codes**

:   `RPC2_SUCCESS`
:   `RPC2_SEFAIL1`
:   `RPC2_SEFAIL2`

### SE\_PrintSEDescriptor

``` c
int SE_PrintSEDescriptor(
    in SE_Descriptor *SDesc,
    in FILE *outFile
);
```

Called when printing debugging information.  You should print out SDesc,
suitably formatted, on outFile.

**Parameters**

:   `SDesc`

    :   The side effect descriptor.

:   `outFile`

    :   The output file.

**Completion Codes**

:   `RPC2_SUCCESS`
:   `RPC2_SEFAIL1`
:   `RPC2_SEFAIL2`

### SE\_SetDefaults

``` c
int SE_SetDefaults(
    out XXX_Initializer *SInit
);
```

Called to set SInit to appropriate default values.

**Parameters**

:   `SInit`

    :   The initializer.

**Completion Codes**

:   `RPC2_SUCCESS`
:   `RPC2_SEFAIL1`
:   `RPC2_SEFAIL2`

### SE\_Activate

``` c
int SE_Activate(
    in XXX_Initializer *SInit
);
```

Called to activate this side effect type.  The body of this procedure should
allocate and fill in a routine vector in the side effect table in file `se.c`.

It should also obtain its initialization parameters from SInit.

**Parameters**

:   `SInit`

    :   The initializer.

**Completion Codes**

:   `RPC2_SUCCESS`
:   `RPC2_SEFAIL1`
:   `RPC2_SEFAIL2`
