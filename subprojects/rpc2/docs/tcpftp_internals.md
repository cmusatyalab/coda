# TCPFTP Internals

## Background

In base RPC2, an SFTP file transfer streams its bytes through the *in-RPC2*
SFTP loop (see [SFTP Internals](sftp_internals.md)): the transmitting
application process (Venus on a client, `codasrv` on a server) reads the file,
segments it into **SFTP_DATA** packets, and drives the windowed
acknowledgement protocol on an LWP thread. The byte movement therefore happens
in the application process, consuming its CPU and an LWP for the duration of
the transfer.

TCPFTP offloads the byte movement out of the application process and onto the
*codatunnel* daemon (`codatunneld`), the forked `libuv` + `gnutls` daemon that
already carries every RPC2 packet over the end-to-end TLS tunnel between a
client and a server. The SFTP *side effect* still travels with the RPC, but
only its **control** payload rides in the request body: a single 8-byte
correlation cookie. Everything else — file identity, tag, direction, offset,
length, and any in-VM buffer — is resolved locally at each end from its own side
effect descriptor, so it never crosses the wire. The cookie is the one value
that pairs the two daemon registrations for a single transfer.

There are thus two channels in play, exactly as in plain SFTP:

- The **RPC2 channel**, used for the control RPC, the retransmissions and
  BUSYs, and — in TCPFTP mode — the 8-byte cookie that starts the transfer.
- The **codatunnel channel**, the daemon-to-daemon TLS tunnel, which now also
  carries the file bytes (previously it carried only encapsulated RPC2
  packets).

The motivation is isolation and scalability: the heavy pread/pwrite loop runs
in the daemon's event loop rather than in the application process, and the
daemon's one-chunk-in-flight discipline is the transfer's backpressure.

## Capability negotiation

TCPFTP is an opt-in-by-capability, not opt-in-by-call. It is always compiled
into `libse`; the only runtime requirement is a running `codatunneld`. The
capability is negotiated in the RPC2 bind handshake, in the `Flags` field of
the packet header:

- `TCPFTP_CAPABLE 0x20`. Set on outgoing packets by a peer whose local
  `codatunneld` is running. The test is `rpc2_tcpftp_capable()`, which returns
  `codatunnel_enabled() && rpc2_tcpftp` (see `globals.c`). The upgrade is
  opt-in via the `RPC2_TCPFTP` env var, read once in `RPC2_Init`
  (`rpc2b.c`) into the `rpc2_tcpftp` global; it is off by default (and when
  the var is `0`/`false`/`no`/`nada`), so a peer only ever advertises the
  capability when the operator has enabled it **and** its daemon is running.
- On the client, the bit is set on **INIT1** before the bind is sent
  (`rpc2a.c`). On the server, when it sees the bit on the client's **INIT1**
  and is itself capable, it marks the connection
  `CE_TCPFTP 0x2` (a per-connection flag in `struct CEntry`) and echoes the
  bit back on **INIT2** / the 3-way-get exchange. On the client, the symmetric
  check marks the connection when the server echoes the bit.
- Once a connection is `CE_TCPFTP`, every subsequent outgoing packet on it is
  stamped with `TCPFTP_CAPABLE`: in `rpc2_SendReliably` (`packet.c`) for the
  single-connection path and in `mrpc_SendPacketsReliably` (`multi1.c`) for the
  MultiRPC path.
- The SFTP side effect reads the negotiated flag at connection setup:
  `SFTP_Bind2` (client) and `SFTP_NewConn` (server) copy
  `(ce->Flags & CE_TCPFTP)` into `se->TcpFtp`, and the shared per-connection
  offload state lives in `se->TcpFtpState` (see `sftp.h`).

A connection that is not `CE_TCPFTP` (old peer, or no daemon running) simply
never sets the bit and the SFTP side effect takes its normal in-process path;
there is no per-transfer negotiation.

## Wire format

On a connection that negotiated TCPFTP, the SFTP side effect marks the request
with a new side effect flag and appends the correlation cookie to the body:

- `SFTP_TCPFTP 0x40` in `Header.SEFlags`. Its comment in `sftp.h` is the
  authoritative description: *on RPC packets, an 8-byte codatunnel correlation
  cookie is appended immediately before the (optional) SFTP parms; the server
  reads it as the tail of the body after parms extraction*.
- The cookie block is a fixed 8 bytes for every supported form. The (de)
  serialization lives in `tcpftp_pack_param_block` /
  `tcpftp_unpack_param_block` (`tcpftp.h`, implemented so it can be unit-tested
  without a live connection or LWP context). The client appends it in
  `SFTP_MakeRPC1` via `sftp_append_cookie`; the server strips it in
  `SFTP_GetRequest`.

Note the server-side asymmetry: `sftp_ExtractParmsFromPacket` shrinks
`Header.BodyLength` but does *not* adjust `Header.SEDataOffset`, so the cookie
is located by measuring `BodyLength` alone and reading the last 8 bytes of the
body after the parms have been peeled off. The cookie rides in the body region
of the RPC packet on the RPC2 channel; it is the only TCPFTP-specific thing
that crosses the application-visible wire.

## File registration: the app-to-daemon control plane

To move bytes over the tunnel, each end must hand its local `codatunneld` an
open file descriptor for the local file. This is the *control plane*, separate
from the byte *data plane*:

- The app calls `codatunnel_file_register(peer, addrlen, fd, offset, length,
  role, &cookie)` (declared in `codatunnel.private.h`; the only production
  caller is the TCPFTP/SFTP binding). The `fd` is delivered to the daemon over
  the local Unix "vside" pipe via `SCM_RIGHTS`, in the same datagram as the
  `CT_FILEREG` envelope. The caller keeps its own handle to `fd`; the daemon
  owns its `dup()`'d copy until the registration is released.
- The `CT_FILEREG` envelope carries a `ct_filereg` (see `ctp.h`):
  `cookie`, `role` (`CT_SOURCE` / `CT_SINK`), `flags` (the `CT_FILREG_LATER`
  bit), the tunnel `peer` address this transfer rides, and `offset` / `length`.
  `length` is the total file length on the source, or the expected byte count
  on the sink.
- `cookie` (8 bytes, network order on the wire) is the key of the daemon's
  shared file table (`cookie.c`), so both daemons of one transfer track the
  same registration. The app's matching cookie table (`cookie.c` on the
  application side) is what `codatunnel_file_wait` blocks on until the daemon
  pushes a terminal `CT_FILEDONE` back over the vside.

The side effect picks the role from `TransmissionDirection`:
`CLIENTTOSERVER` means this end is the **source**, `SERVERTOCLIENT` means the
**sink**. For an `FILEINVM` *sink*, the local file is an empty unlinked
temporary spool that the daemon `pwrite()`s; the side effect keeps that spool
fd open in `TcpFtpState.VmFd` so its finalize hook can seek + read the bytes
back into the in-VM buffer.

## The LATER bit, and which end drives

A single daemon only sees its own `FILEREG`, so it cannot tell on its own which
end registered first. The `CT_FILREG_LATER` bit resolves that. The RPC-server
side (the `CheckSE` path) is *always* the later end — it passes the client's
cookie back through `*cookie` as-is (the "non-zero in, use as-is" convention),
and that single fact is what sets the bit.

That one bit drives both directions and keeps each end in the right posture:

| Direction       | Client end                  | Server end                    |
| --------------- | --------------------------- | ----------------------------- |
| **Push** (C→S)  | earlier `CT_SOURCE`         | later `CT_SINK` — the puller  |
| **Fetch** (S→C) | earlier `CT_SINK`           | later `CT_SOURCE` — the pump  |

- In a **push** (client to server), the server's sink is *later* and therefore
  drives the transfer by issuing `CT_TRANSFER_REQUEST`s; the client's source
  is *earlier* and sits passively, serving whatever it is asked for.
- In a **fetch** (server to client), the server's source is *later* and
  therefore starts the pump immediately (its sink pre-registered); the client's
  sink is *earlier* and stays passive.

The gate is applied in two places in the daemon: a `CT_SINK` issues a
follow-on `CT_TRANSFER_REQUEST` only if it is `later`; a `CT_SOURCE` answers a
`CT_TRANSFER_REQUEST` only if it is *not* `later`. This is what keeps the fetch
client sink quiet and the push server sink active, and it is the reason the
data path is asymmetric.

## Data plane: the daemon-to-daemon protocol

The daemon-to-daemon records ride in the body region of a `ctp_t` envelope over
the TLS channel. The relevant opcodes (`ctp.h`):

- `CT_TRANSFER_READY 5` — the sink (later end) is open; proceed.
- `CT_TRANSFER_DATA 6` — file bytes at `{cookie, offset}`, followed in the same
  record by the payload.
- `CT_TRANSFER_EOF 7` — no more bytes for this cookie.
- `CT_TRANSFER_ERROR 8` — a local open/transfer failed.
- `CT_TRANSFER_REQUEST 9` — the sink asks the source for bytes at
  `{cookie, offset, len}` (`len <= CT_CHUNKMAX`).

All 8-byte fields travel in network byte order. `CT_CHUNKMAX` is sized so a
full-chunk `CT_TRANSFER_DATA` record is exactly `CT_TLSMAXPAYLOAD` (65535),
keeping a full chunk inside a single TLS fragment so `gnutls_record_send` will
not split it. `CT_MAX_RECORD` is the largest record the daemon accepts on a
channel.

### Push: sink-driven pull

For a client-to-server transfer the server's later sink opens the file, sends
the first `CT_TRANSFER_REQUEST`, and then, after each chunk lands
(`ct_transfer_data_arrived`), issues the next `CT_TRANSFER_REQUEST` for
`CT_CHUNKMAX` more bytes. The client's earlier source answers each
`CT_TRANSFER_REQUEST` (`ct_transfer_request_arrived`): it `pread()`s up to the
requested length at the requested offset and replies with a
`CT_TRANSFER_DATA`. When the bytes served reach the source's registered length
it sends the definitive `CT_TRANSFER_EOF` and reports `CT_FILEDONE` locally so
its own finalize returns. A REQUEST that no longer has a live source record —
or that targets a *later* (pump-driven) fetch source — is dropped.

### Fetch: source-driven pump

For a server-to-client transfer the server's later source runs a pump
(`ct_pump_do_start`). The pump works one chunk at a time, entirely on the event
loop: it `pread()`s a chunk from the source file and fires it at the channel
with a completion hook; when that chunk's bytes hit TCP, the send worker
re-arms the pump and it reads the next chunk. The client's earlier sink stays
passive, writing each `CT_TRANSFER_DATA` into its file (or spool). The pump
emits `CT_TRANSFER_EOF` once the file is exhausted and finishes.

### Backpressure: one chunk in flight

In both directions exactly one chunk (`CT_CHUNKMAX`) is ever in flight. The
next chunk is only read after the previous one has landed on TCP, and that is
the transfer's backpressure. This also means nothing in the pump ever blocks
the loop: a `libuv` blocking wait would deadlock the daemon, because a
`gnutls` send only returns once the loop has done the TCP write, and the loop
cannot do the TCP write while it is inside that wait.

## Terminal states and error handling

- A daemon pushes a terminal status to its local app over the vside as
  `CT_FILEDONE` (`cookie`, `nbytes`, `status`). The status is in the SFTP
  error space (`CT_STATUS_*`, 0 == success); the app's SE layer maps it back
  to `SE_SUCCESS` / `SE_FAILURE` at finalize. `ct_errno_to_status` maps local
  `errno` into these; `CT_STATUS_TIMEOUT` means channel death or
  cancellation.
- Completion is **EOF-driven**: the transfer is over when the source sends
  `CT_TRANSFER_EOF`, never inferred from a short chunk. A sink that sees
  `CT_TRANSFER_EOF` (or `CT_TRANSFER_ERROR`) finalizes its record, and if the
  byte count written does not match the registered length it downgrades the
  status to `CT_STATUS_IOERR`.
- A peer-originated `CT_TRANSFER_ERROR` is the *legitimate* terminal state
  (the source's pump failed), so the channel stays up for other traffic. By
  contrast a *torn stream* — an unknown record, an offset gap/overlap, or a
  `pwrite` failure on the sink — sends `CT_TRANSFER_ERROR` to the peer, closes
  the transfer with the local app, and destroys the channel.
- **Channel death.** When a tunnel channel tears down, every pending transfer
  that rode it can never complete (no REQUEST/DATA/EOF will ever arrive).
  `free_dest` calls `ct_fail_dest_transfers`, which walks the whole cookie
  table via `ct_cookie_foreach` and fails every record whose peer matches the
  dead destination with `CT_STATUS_TIMEOUT` (a terminal `CT_FILEDONE` before
  the record is freed). This is what closes both hangs: a server-side sink
  waiting on a dead client, and a client-side source waiting on a dead server.
  The walk is idempotent, and `ct_rec_free_now` is a no-op while a pump still
  holds a `refs` reference to the record.

## Side effect wiring

The production path is the SFTP side effect with the offload turned on;
`tcpftp.h` also defines a standalone `TCPFTP` SE (`TCPFTP_Activate`) and the
shared file-registration helpers. Both share the same per-connection state,
`struct TcpFtpState { uint64_t Cookie; int VmFd; int GotBlock; }`, embedded in
the standalone entry and in the SFTP `SFTP_Entry`.

On the SFTP path (`sftp1.c`), once a connection is `TcpFtp`:

- `SFTP_MakeRPC1` (client) resolves the local file, registers it with the
  daemon (`tcpftp_register_local`, in role source/sink from
  `TransmissionDirection`), packs the cookie and appends it, sets
  `SFTP_TCPFTP`, and still piggybacks the SFTP parms on the first call. If the
  codatunnel is unusable it *falls back to the in-VM SFTP path* rather than
  retrying.
- `SFTP_GetRequest` (server) peels the 8-byte cookie off the tail of the body
  after the parms, records `GotBlock`.
- `SFTP_InitSE` (server) skips the in-VM pump entirely (`GotBlock` is set) —
  there is no in-process file loop; the daemon does the work.
- `SFTP_CheckSE` (server) registers the local file with the client's cookie,
  finalizes (`tcpftp_finalize` waits for the daemon's terminal status and,
  for an in-VM sink, drains the spool back into the buffer), and frees the SFTP
  IO.
- `SFTP_MakeRPC2` (client) finalizes on the reply, and `sftp_FreeSEntry`
  releases any in-flight cookie and spool fd so an aborted transfer cannot leak
  the daemon slot.

The standalone `TCPFTP` SE (`tcpftp1.c`) performs the same register → wait →
finalize dance through its own `MakeRPC1`/`MakeRPC2`/`GetRequest`/`CheckSE`
entry points and the same `tcpftp_register_local` / `tcpftp_finalize`
helpers, with the shared `TcpFtpState` in the `TCPFTP_Entry`.

## References

- [SFTP Internals](sftp_internals.md) — the in-process file loop that TCPFTP
  replaces.
- [Adding New Kinds of Side Effects](se_internals.md) — the `SE_*` entry-point
  template the TCPFTP and SFTP side effects implement.
- `ctp.h` — the daemon wire protocol (opcodes, `ct_filereg`, chunk sizing).
- `cookie.h` / `cookie.c` — the cookie table shared by both daemons and the
  app, and its `ct_cookie_foreach` teardown walk.
- `tcpftp.h` — the per-connection state, registration/finalize helpers, and the
  cookie param-block (de)serialization.
