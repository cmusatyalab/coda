# RP2Gen Stub Generator

## Introduction

_RP2GEN_ takes a description of a procedure call interface and generates stubs
to use the RPC2 package, making the interface available on remote hosts.
RP2GEN is designed to work with a number of different languages (C, FORTRAN 77,
PASCAL), however, only the C interface is currently implemented.

RP2GEN also defines a set of external data representations for RPC types.
These representations are defined at the end of this document in the section
entitled _External Data Representations_.  Any program wishing to communicate
with a remote program using the RP2GEN semantics must obey these representation
standards.

## Usage

RP2GEN is invoked as follows:

``` sh
rp2gen file.rpc2
```

_file.rpc2_ is the file containing the description of the interface.  Normally,
these files have the extension `.rpc2`.  RPGen creates three files named:
`file.client.c`, `file.server.c`, and `file.h`, where `file` is the name of the
file without the extension and pathname prefix.  Thus:

``` sh
rp2gen samoan.rpc2
```

would yield the files: `samoan.client.c`, `samoan.server.c`, and `samoan.h`.

A person wanting to provide a package remotely writes his package with a normal
interface.  The client programmer writes his code to make normal calls on the
interface.  Then the client program is linked with:

``` sh
ld ... file.client.o /usr/andrew/lib/librpc2.a ...
```

and the server program with

``` sh
ld ... file.server.o /usr/andrew/lib/librpc2.a ...
```

The server module provides a routine, the `ExecuteRequest` routine, that will
decode the parameters of the request and make an appropriate call on the
interface.  (The routine is described below in the language interface
sections.)  The client module translates calls on the interface to messages
that are sent via the RPC2 package.  The `.h` file contains type definitions
that RP2GEN generated from the type definitions in the input file, and
definitions for the op-codes used by RP2GEN.  This file, which is automatically
included in the server and client files, may be included by any other module
that needs access to these types.

## Format of the Description File

The syntax of a description file is shown below in EBNF format, non-terminals
are represented by long\_names and literals are represented by "quoted strings".

``` ebnf
file            = prefixes header_line default_timeout decl_or_proc_list ;
prefixes        = empty | prefix | prefix prefix ;
prefix          = "Server Prefix" string ";" | "Client Prefix" string ";" ;
header_line     = "Subsystem" subsystem_name ";" ;
subsystem_name  = string ;
string          = '"' zero_or_more_ascii_chars '"' ;
default_timeout = "Timeout" "(" id_number ");" | empty ;
decl_or_proc_list = decl_or_proc | decl_or_proc decl_or_proc_list ;
decl_or_proc    = include | define | typedef | procedure_description ;
include         = "#include" '"' file_name '"' ;
define          = "#define" identifier number ;
typedef         = "typedef" rpc2_type identifier array_spec ";" ;
rpc2_type       = type_name | rpc2_struct | rpc2_enum ;
type_name       = "RPC2_Integer" | "RPC2_Unsigned" | "RPC2_Byte" |
                  "RPC2_String" | "RPC2_CountedBS" | "RPC2_BoundedBS" |
                  "SE_Descriptor" | "RPC2_EncryptionKey" | identifier ;
rpc2_struct     = "RPC2_Struct" "{" field_list "}" ;
field_list      = field | field field_list ;
field           = type_name identifier_list ";" ;
identifier_list = identifier | identifier "," identifier_list ;
rpc2_enum       = "RPC2_Enum" "{" enum_list "}" ;
enum_list       = enum "," enum_list | enum ;
enum            = identifier "=" number ;
array_spec      = empty | "[" id_number "]" ;
id_number       = number | identifier '
procedure_description = proc_name "(" formal_list ")"
                        timeout_override new_connection ";" ;
proc_name       = identifier ;
formal_list     = empty | formal_parameter | formal_parameter "," formal_list ;
formal_parameter = usage type_name parameter_name ;
usage           =  "IN" | "OUT" | "IN OUT" ;
parameter_name  = identifier ;
timeout_override = "Timeout" "(" id_number ")" | empty ;
new_connection  = "NEW_CONNECTION" | empty ;
empty           = ;
```

In addition to the syntax above, text enclosed in `/*` and `*/` is treated as a
comment and ignored.  Appearances of an include statement will be replaced by
the contents of the specified file.
All numbers are in decimal and may be preceded by a single hyphen (**-**)
character.
Identifiers follow C syntax except that the underline character (**\_**), may
not begin an identifier.

!!! note
    A particular language interface defines what identifiers may actually be
    used in various contexts.

The following are reserved words in RP2GEN: **server**, **client**,
**prefix**, **subsystem**, **timeout**, **typedef**, **rpc2\_struct**,
**rpc2\_enum**, **in** and **out**.  Case is ignored for reserved words, so
that, for example, **subsystem** may be spelled as **SubSystem** if desired.
Case is not ignored, however, for identifiers.

!!! note
    The predefined type names (RPC2\_Integer , RPC2\_Byte , etc.) are
    identifiers and must be written exactly as given above.

The _prefixes_ may be used to cause the names of the procedures in the
interface to be prefixed with a unique character string.  The line:

``` rpc2
Server Prefix "test";
```

will cause the server file to assume that the name of the server interface
procedure _name_ is `test_name`.  Likewise, the statement:

``` rpc2
Client Prefix "real";
```

affects the client interface.  This feature is useful in case it is necessary
to link the client and server interfaces together.  Without this feature, name
conflicts would occur.

The _header\_line_ defines the name of this subsystem.  The subsystem name is
used in generating a unique for the `execute request` routine.

The _default\_timeout_ is used in both the server and client stubs.  Both are
specified in seconds.  Zero is interpreted as an infinite timeout value.  The
value specifies the timeout value used on `RPC2_MakeRPC()` and
`RPC2_SendResponse()` calls in the client and server stubs respectively.  The
timeout parameter may be overriden for individual procedures by specifying a
_timeout\_override_.

!!! note
    The timeouts apply to each individual Unix blocking system call, not to the
    entire RPC2 procedure.

The `new_connection` is used to designate at most one server procedure that
will be called when the subsystem receives the initial RPC2 connection.  The
new connection procedure must have 4 arguments in the following order with the
following usages and types:

``` rpc2
IN RPC2_Integer   SideEffectType
IN RPC2_Integer   SecurityLevel
IN RPC2_Integer   EncryptionType
IN RPC2_CountedBS ClientIdent
```

where `SideEffectType`, `SecurityLevel`, `EncryptionType`, and `ClientIdent`
have the values that were specified on the clients call to `RPC2_Bind()`.

!!! note

    RP2Gen will automatically perform an `RPC2_Enable()`  call at the end of
    this routine.  If no new connection procedure is specified, then the call
    to the _execute request_ routine with the initial connection request will
    return `RPC2_FAIL`.

The _usage_ tells whether the data for the parameter is to be copied in, copied
out, or copied in both directions.  The _usage_ and _type\_name_ specifications
together tell how the programmer should declare the parameters in the server
code.

### Example 1: Common Definitions for Coda File System

``` rpc2
/* Include file common to callback.rpc2, vice.rpc2 and res.rpc2 */

typedef RPC2_Unsigned VolumeId;
typedef VolumeId      VolId;
typedef RPC2_Unsigned VnodeId;
typedef RPC2_Unsigned Unique;

typedef RPC2_Struct {
    VolumeId Volume;
    VnodeId  Vnode;
    Unique   Unique;
} ViceFid;

typedef RPC2_Struct {
    RPC2_Unsigned Host;
    RPC2_Unsigned Uniquifier;
} ViceStoreId;

typedef RPC2_Struct {
    RPC2_Integer Site0;
    RPC2_Integer Site1;
    RPC2_Integer Site2;
    RPC2_Integer Site3;
    RPC2_Integer Site4;
    RPC2_Integer Site5;
    RPC2_Integer Site6;
    RPC2_Integer Site7;
} ViceVersionArray;

typedef RPC2_Struct {
    ViceVersionArray Versions;
    ViceStoreId      StoreId;
    RPC2_Unsigned    Flags;
} ViceVersionVector;

typedef RPC2_Unsigned UserId;
typedef RPC2_Unsigned FileVersion;
typedef RPC2_Unsigned Date;
typedef RPC2_Integer  Rights;

typedef RPC2_Enum {
    Invalid = 0,
    File = 1,
    Directory = 2,
    SymbolicLink = 3
} ViceDataType;

typedef RPC2_Enum {
    NoCallBack = 0,
    CallBackSet = 1,
    BidFidReleased = 3
} CallBackStatus;

typedef RPC2_Struct {
    RPC2_Unsigned     InterfaceVersion;
    ViceDataType      VnodeType;
    RPC2_Integer      LinkCount;
    RPC2_Unsigned     Length;
    FileVersion       DataVersion;
    ViceVersionVector VV;
    Date              Date;
    UserId            Author;
    UserId            Owner;
    CallBackStatus    CallBack;
    Rights            MyAccess;
    Rights            AnyAccess;
    RPC2_Unsigned     Mode;
    VnodeId           vparent;
    Unique            uparent;
} ViceStatus;
```

### Example 2: The Coda Resolution Subsystem Interface

``` rpc2
/* res.rpc2 - Defines the resolution subsystem interface
 * Created by Puneet Kumar, June 1990
 */
server prefix "RS";
client prefix "Res";

Subsystem "resolution";

#define RESPORTAL          1361
#define RESOLUTIONSUBSYSID 5893

/* Return codes from the servers on resolution subsystem */
#define RES_FAILURE     -512
#define RES_SUCCESS     0
#define RES_TIMEDOUT    -513
#define RES_NOTRUNT     -514
#define RES_BADOPLIST   -515

#include "vcrcommon.rpc2"

typedef RPC2_Struct {
    RPC2_Integer    status;
    RPC2_Unsigned   Author;
    RPC2_Unsigned   Owner;
    RPC2_Unsigned   Date;
    RPC2_Unsigned   Mode;
} ResStatus;

typedef RPC2_Struct {
    RPC2_Integer        LogSize;
    ViceVersionVector   VV;
} ResVolParm;

typedef RPC2_Enum {
    FetchStatus = 0,
    FetchSData = 1
} ResFetchType;

typedef RPC2_Enum {
    ResStoreStatus = 0,
    ResStoreData = 1
} ResStoreType;

COP2 (
    IN ViceStoreId StoreId,
    IN ViceVersionVector UpdateSet
);

NewConnection (
    IN RPC2_Integer SideEffectType,
    IN RPC2_Integer SecurityLevel,
    IN RPC2_Integer EncryptionType,
    IN RPC2_CountedBS ClientIdent
) NEW_CONNECTION;

ForceFile (
    IN ViceFid Fid,
    IN ResStoreType Request,
    IN RPC2_Integer Length,
    IN ViceVersionVector VV,
    IN ResStatus Status,
    IN OUT SE_Descriptor BD
);

LockAndFetch (
    IN ViceFid Fid,
    IN ResFetchType Request,
    OUT ViceVersionVector VV,
    OUT RPC2_Integer logsize
);

UnlockVol (
    IN VolumeId Vid
);

MarkInc (
    IN ViceFid Fid
);

FetchFile (
    IN ViceFid Fid,
    IN RPC2_Unsigned PrimaryHost,
    OUT ResStatus Status,
    IN OUT SE_Descriptor BD
);

ForceDirVV (
    IN ViceFid Fid,
    IN ViceVersionVector VV
);

DoForceDirOps (
    IN ViceFid Fid,
    IN ViceStatus status,
    IN OUT RPC2_CountedBS AccessList,
    OUT RPC2_Integer rstatus,
    IN OUT SE_Descriptor sed
);

GetForceDirOps (
    IN ViceFid Fid,
    OUT ViceStatus status,
    IN OUT RPC2_CountedBS AccessList,
    IN OUT SE_Descriptor sed
);

FetchLog (
    IN ViceFid Fid,
    OUT RPC2_Integer logsize,
    IN OUT SE_Descriptor sed
);

DirResPhase2 (
    IN ViceFid Fid,
    IN ViceStoreId logid,
    OUT ViceStatus status,
    IN RPC2_BoundedBS pbinc
);

DirResPhase1 (
    IN ViceFid Fid,
    IN RPC2_Integer size,
    IN OUT ViceStatus status,
    IN OUT RPC2_BoundedBS piggyinc,
    IN OUT SE_Descriptor sed
);

DirResPhase3 (
    IN ViceFid Fid,
    IN ViceVersionVector UpdateSet,
    IN OUT SE_Descriptor sed
);
```

## Command Line Parameters

In addition, several command line flags are available to modify the behavior of
rp2gen:

| Flag | Description |
| ---- | ----------- |
| -c `file` | Specify the name of the client .c file. |
| -s `file` | Specify the name of the server .c file. |
| -h `file` | Specify the name of the header file. |
| -m `file` | Specify the name of the MultiRPC stub file. |
| -I `path` | Additional path to look for included files. |
| -e, -neterrors | Translate system-specific error codes to caller's system-specfic codes. |
| -cplusplus | Generate C++ compatible code in .cc files. |

## The C Interface

This section describes the **C interface** generated by RP2GEN.  The following
table shows the relationship between RP2GEN parameter declarations and the
corrseponding C parameter declarations.

| RPC2 Type | in | out | in out |
| --------- | -- | --- | ------ |
| RPC2\_Integer  | long | long \* | long \* |
| RPC2\_Unsigned  | unsigned long | unsigned long \* | unsigned long \* |
| RPC2\_Byte  | unsigned char | unsigned char \* | unsigned char \* |
| RPC2\_String  | unsigned char \* | unsigned char \* | unsigned char \* |
| RPC2\_CountedBS  | RPC2\_CountedBS \* | RPC2\_CountedBS \* | RPC2\_CountedBS \* |
| RPC2\_BoundedBS  | RPC2\_BoundedBS \* | RPC2\_BoundedBS \* | RPC2\_BoundedBS \* |
| RPC2\_EncryptionKey  | RPC2\_EncryptionKey | RPC2\_EncryptionKey \* | RPC2\_EncryptionKey \* |
| SE\_Descriptor | _illegal_ | _illegal_ | SE\_Descriptor \* |
| RPC2\_Enum _name_ | _name_ | _name_ \* | _name_ \* |
| RPC2\_Struct _name_ | _name_ \* | _name_ \* | _name_ \* |
| RPC2\_Byte _name[...]_ | _name_ | _name_ | _name_ |

/// table-caption
RP2Gen representation of parameters
///

In all cases it is the caller's responsibility to allocate storage for all
parameters.  This means that for `IN` and `IN OUT` parameters of a non-fixed
type, it is the callee's responsibility to ensure that the value to be copied
back to the caller does not exceed the storage allocated by the callee.

The caller must call an RPC2 procedure with an initial implicit argument of
type `RPC2_Handle` that indicates the destination address(es) of the target
process(es).  The callee must declare the C routine that corresponds to an RPC2
procedure with an initial implicit argument of type `RPC2_Handle`.  Upon
invocation, this argument will be bound to the address of a handle that
indicates the address of the caller.

RP2GEN also generates a routine that serves to decode an RPC2 request.  The
name of this routine is `subsystem_name_ExecuteRequest`, and it is invoked as
follows:

``` c
int subsystem_name_ExecuteRequest(
    RPC2_Handle cid,
    RPC2_PacketBuffer *Request,
    SE_Descriptor *bd
);
```

This routine will unmarshall the arguments and call the appropriate interface
routine.  The return value from this routine will be the return value from the
interface routine.

The client program is responsible for actually making the connection with the
server and must pass the connection id as an additional parameter (the first)
on each call to the interface.

## External Data Representations

This section defines the external data representation used by RP2GEN, that is,
the representation that is sent out over the wire.  Each item sent over on the
wire is required to be a multiple of 4 (8-bit) bytes.  (Items are padded as
necessary to achieve this constraint.)  The bytes of an item are numbered `0`
through `n - 1` (where `n mod 4 = 0`).  The bytes are read and written such
that byte `m` always precedes byte `m + 1`.

RPC2\_Integer

:   An RPC2\_Integer is a 32-bit item that encodes an integer represented in
    twos complement notation.  The most significant byte of the integer is 0,
    and the least significant byte is 3.

RPC2\_Unsigned

:   An RPC2\_Unsigned is a 32-bit item that encodes an unsigned integer.  The
    most significant byte of the integer is 0, the least significant byte is 3.

RPC2\_Byte

:   An RPC2\_Byte is transmitted as a single byte followed by three padding
    bytes.

RPC2\_String

:   An RPC2\_String is a C-style null-terminated character string.  It is sent
    as an RPC2\_Integer indicating the number of characters to follow, not
    counting the null byte, which is, however, sent.  This is followed by bytes
    representing the characters (padded to a multiple of 4), where the first
    character (i.e., farthest from the null byte) is byte 0.  An RPC2\_String
    of length 0 is representing by sending an RPC2\_Integer with value 0,
    followed by a 0 byte and three padding bytes.

RPC2\_CountedBS

:   An RPC2\_CountedBS is used to represent a byte string of arbitrary length.
    The byte string is not terminated by a null byte.  An RPC2\_CountedBS is
    sent as an RPC2\_Integer  representing the number of bytes, followed by the
    bytes themselves (padded to a multiple of 4 .  The byte with the lowest
    address is sent as byte 0.

RPC2\_BoundedBS

:   An RPC2\_BoundedBS is intended to allow you to remotely play the game that
    C programmers play: allocate a large buffer, fill in some bytes, then call
    a procedure that takes this buffer as a parameter and replaces its contents
    by a possibly longer sequence of bytes.  An RPC2\_BoundedBS is transmitted
    as two RPC2\_Integer s representing the maximum and current lengths of the
    byte strings.  This is followed by the  bytes representing the contents of
    the buffer (padded to a multiple of 4).  The byte with the lowest address
    is byte 0.

RPC2\_EncryptionKey

:   An RPC2\_EncryptionKey is used to transmit an encryption key (surprise!).
    A key is sent as a sequence of RPC2\_KEYSIZE  bytes, padded to a multiple
    of 4.  Element 0 of the array is byte 0.

SE\_Descriptor

:   Objects of type SE\_Descriptor are never transmitted.

RPC2\_Struct

:   An RPC2\_Struct is transmitted as a sequence of items representing its
    fields.  The fields are sent in textual order of declaration (i.e., from
    left to right and top to bottom).  Each field is sent using, recursively,
    its RPC2 representation.

RPC2\_Enum

:   An RPC2\_Enum has the same representation has an RPC2\_Integer , and the
    underlying integer used by the compiler is transmitted as the value of an
    RPC2\_Enum.

    !!! note
        In C, this underlying value may be specified by the user.  This is
        recommended practice.

Array

:   The total number of bytes transmitted for an array must be a multiple of 4.
    However, the number of bytes sent for each element depends on the type of
    the element.

    Currently, only arrays of RPC2\_Byte are defined.  The elements of such an
    array are each sent as a single byte (no padding), with array element,
    `n - 1`, preceding element, `n`.
