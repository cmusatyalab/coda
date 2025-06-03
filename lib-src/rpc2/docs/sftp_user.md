# SFTP: A Side Effect for Bulk Data Transfer

## General Usage

SFTP is a protocol that allows a byte stream to be transferred efficiently from
point A to point B, using optimizations such as streaming for large files and
piggybacking for small files. These optimizations are completely transparent to
the programmer. The byte stream to be transferred can be a file in the file
system or a file in virtual memory. A file in VM at one end need not be
transferred into a file in VM at the other end; any combination is possible. An
SFTP transfer can take place from either an RPC2 server to an RPC2 client or
the other way around.  However the request for the transfer is always initiated
by the client.  To avoid confusion, we will refer to the sending entity
_source_ and the receiving entity _sink_.

The average user of SFTP need not be aware of the details of SFTP related to
acknowledgements, flow control etc. All he needs to specify are the details of
the file transfer such as the name of the file, the transmission direction etc.
These are specified by the user in a data structure called a _Side Effect
Descriptor_. The side effect descriptor is used both at the client and the
server ends to describe the details of the byte stream transfer. The side
effect descriptor is not transmitted: each side provides its own local version.

The most relevant of the side effect descriptors fields are given below.

Tag

:   This describes the type of side effect used. Currently the only side effect
    that has been implemented is the SFTP file transfer side effect.

Value

:   This is a union containing descriptor definitions for each type of side
    effect.  The Tag field serves as the discriminant of the union.  Currently,
    there is only one member of the union, corresponding to SFTP.

Value.SmartFTPD.TransmissionDirection

:   Specifies if the transfer is from the server to the client (SERVERTOCLIENT)
    or from the client to the server (CLIENTTOSERVER).

Value.SmartFTPD.SeekOffset

:   Specifies the initial position from where a file should be read from, or
    written to. This is useful, for instance, when bytes are appended to a file.

Value.SmartFTPD.ByteQuota

:   Specifies the  maximum number of data bytes to be sent or received.  A
    value of -1 implies a limit of infinity.  The RPC call fails if an attempt
    is made to transfer more bytes.

Value.BytesTransferred

:   Is an output parameter.  On completion of the file transfer, it specifies
    the number of bytes actually transferred.

Value.SmartFTPD.Tag

:   Indicates whether the file to be transferred is specified by its name, its
    inode number, or by its VM address.

Value.SmartFTPD.FileInfo

:   Is a union type, discriminated by the previous Tag field, that specifies
    the local identity of the file to be transferred.

    !!! note
        There is no information of the remote identity of the file.

## An Example

An example of an RPC subsystem which uses SFTP is given in the following
section. In describing the use of SFTP, we will refer to this example and
outline the steps used. The client-server code using SFTP is very similar to
that of an RPC which does not use SFTP.  Hence , we will only describe the
additional steps needed when making an RPC which uses SFTP.

### rcat Subsystem Interface

[rcat.rpc](examples/rcat.rpc.md)

### Server Code

[rcat_srv.c](examples/rcat_srv.c.md)

- Initialize the SFTP package.  This is done by first declaring a data
  structure of type SFTP\_Initializer.  The fields of the SFTP\_Initializer
  specify parameters of the SFTP file transfer protocol, and include items such
  as the window size, packet size etc. These can be set by the user, or can be
  set to default values by calling the `SFTP_SetDefaults` primitive. The
  SFTP\_Initializer is then activated using the `SFTP_Activate` primitive.

    !!! note
        The SFTP package should be activated before the RPC2 package is
        initialized.

- The routine which actually transfers the file does so by first declaring a
  data structure of type SE\_Descriptor and filling in the values.  It then
  initiates the file transfer by calling the `RPC2_InitSideEffect` primitive.

- The `RPC2_CheckSideEffect` primitive can then be used to check the status of
  the side effect.  Note that in the case of SFTP, the actual file transfer
  only occurs on this call.

### Client Code

[rcat_clnt.c](examples/rcat_clnt.c.md)

- As with the server, the SFTP package is initialized by declaring an
  SFTP\_Initializer, and by calling the `SFTP_SetDefaults` and
  `SFTP_Activate` primitives.

- Set the SideEffectType field in RPC2\_BindParms to SMARTFTP.

- Declare a data structure of type SE\_Descriptor.  Set the Tag field to
  SMARTFTP, and fill in the rest of the fields.

- Make the RPC.
