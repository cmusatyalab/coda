# Multicast

## General Usage

!!! note

    Not written yet.

## Runtime Calls

### RPC2\_CreateMgrp

``` c
int RPC2_CreateMgrp(
    out RPC2_Handle *MgroupHandle,
    in RPC2_McastIdent *McastAddr,
    in RPC2_PortIdent *McastPort,
    in RPC2_SubsysIdent *Subsys,
    in RPC2_Integer SecurityLevel,
    in RPC2_EncryptionKey SessionKey,
    in RPC2_Integer EncryptionType,
    in long SideEffectType
);
```

Creates a new RPC2 mgroup identifier, unique to a particular &lt;multicast
address, port, subsystem&gt; combination, and returns it to the caller.  Once the
identifier has been assigned, RPC2 mgroup members may be added and deleted via
`RPC2_AddToMgrp` and `RPC2_RemoveFromMgrp` calls. The security level,
encryption type and side effect types of each added connection must match the
corresponding parameters of this `RPC2_CreateMgrp` call.

**Parameters**

:   `MgroupHandle`

    :   An integer, unique to a specific client, returned by the call. When
        combined with the client's network address, it uniquely identifies this
        mgroup connection. This is not necessarily a small-valued integer.

:   `McastAddr`

    :   The IP multicast address of the desired multicast group.  This is the
        address that a server must use in a join request.

:   `McastPort`

    :   The IP multicast port of the desired multicast group.  This is the port
        that a server must use in a join request.

:   `Subsys`

    :   The subsystem identifier of the desired multicast group.  This is the
        subsystem that a server must use in a join request.

:   `SecurityLevel`

    :   The security level of the desired multicast group.  This is the
        security level that a server must use in a join request.

:   `SessionKey`

    :   The session key of the desired multicast group.  This is the session
        key that a server must use in a join request.

:   `EncryptionType`

    :   The encryption type of the desired multicast group.  This is the
        encryption type that a server must use in a join request.

:   `SideEffectType`

    :   The side effect type of the desired multicast group.  This is the side
        effect type that a server must use in a join request.

**Completion Codes**

:   `RPC2_SUCCESS`

    :   All went well.

:   `RPC2_SEFAIL1`

    :   Side effect routines reported temporary failure.

:   `RPC2_SEFAIL2`

    :   Side effect routines reported critical failure.

:   `RPC2_FAIL`

    :   Some other mishap occurred.

### RPC2\_AddToMgrp

``` c
int RPC2_AddToMgrp(
    in RPC2_Handle MgroupHandle,
    in RPC2_Handle ConnHandle
);
```

Adds ConnHandle to the mgroup associated with MgroupHandle.  RPC2 contacts the
remote site to initialize its mgroup connection information.  The security
level, encryption type and side effect type of the connection being added must
match that specified when Mgroup was defined.

**Parameters**

:   `MgroupHandle`

    :   Identifies the mgroup to which the new server should be added.

:   `ConnHandle`

    :   Identifies the connection to be added to the mgroup.

**Completion Codes**

:   `RPC2_SUCCESS`

    :   All went well.

:   `RPC2_NOMGROUP`

    :   *MgroupHandle* is not a valid mgroup.

:   `RPC2_NOCONNECTION`

    :   *ConnHandle* is not a valid connection.

:   `RPC2_BADSECURITY`

    :   SecurityLevel or EncryptionType of *ConnHandle* does not match that of
        *MgroupHandle*.

:   `RPC2_DUPLICATEMEMBER`

    :   *ConnHandle* is already a member of *MgroupHandle*.

:   `RPC2_MGRPBUSY`

    :   A call is in progress on *MgroupHandle*.

:   `RPC2_CONNBUSY`

    :   A call is in progress on *ConnHandle*.

:   `RPC2_SEFAIL1`

    :   Error code returned by side effect routine.

:   `RPC2_SEFAIL2`

    :   Error code returned by side effect routine.

:   `RPC2_NAKED`

    :   The remote site corresponding to *ConnHandle* is not a member of the
        multicast group.

:   `RPC2_FAIL`

    :   Some other mishap occurred.

### RPC2\_RemoveFromMgrp

``` c
int RPC2_RemoveFromMgrp(
    in RPC2_Handle MgroupHandle,
    in RPC2_Handle ConnHandle
);
```

Removes ConnHandle from the mgroup associated with MgroupHandle.

**Parameters**

:   `MgroupHandle`

    :   Identifies the mgroup to be shrunk.

:   `ConnHandle`

    :   Identifies the connection to be removed from the mgroup.

**Completion Codes**

:   `RPC2_SUCCESS`

    :   All went well.

:   `RPC2_NOMGROUP`

    :   *MgroupHandle* does not refer to a valid mgroup connection.

:   `RPC2_NOCONNECTION`

    :   *ConnHandle* is bogus.

:   `RPC2_NOTGROUPMEMBER`

    :   *ConnHandle* is not a member of *MgroupHandle*.

:   `RPC2_MGRPBUSY`

    :   A call is in progress on *MgroupHandle*.

:   `RPC2_FAIL`

    :   Some strange mishap occurred.

### RPC2\_DeleteMgrp

``` c
int RPC2_DeleteMgrp(
    in RPC2_Handle MgroupHandle
);
```

Deletes a RPC2 mgroup, removing any existing members before deletion

**Parameters**

:   `MgroupHandle`

    :   Identifies the mgroup to be deleted.

**Completion Codes**

:   `RPC2_SUCCESS`

    :   All went well.

:   `RPC2_NOMGROUP`

    :   *MgroupHandle* does not refer to a valid mgroup connection.

:   `RPC2_MGRPBUSY`

    :   A call is in progress on *MgroupHandle*.

:   `RPC2_FAIL`

    :   Some other mishap occurred.
