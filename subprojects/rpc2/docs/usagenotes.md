# Usage and Implementation Notes

## Current Limitations

- Only one port is allowed in `RPC2_Init`.  This should be a list eventually.
- SmartFTP is the only side-effect type currently supported.
- getsubsysbyname() is a fake routine.  It knows about `Vice2-FileServer`, and
  `Vice2-CallBack`.
- At present the multicast routines work only on RTs running Mach.  The
  non-multicast MultiRPC routines work on all architectures.

## Where to Find Files at Carnegie Mellon

The RPC2 release consists of a set of include files (rpc2.h, se.h),
a set of libraries (librpc2.a and libse.a), and the RP2Gen binary (rp2gen).

The RPC2 releases are available for RTs, Sun-3s, Vaxen, Pmaxen running
Andrew and Mach, and Next running Andrew.  (Systypes `rt_r3`, `rt_mach`,
`sun3_35`, `sun3_mach`, `vax_3`, `vax_mach`, `pmax_3`, `pmax_mach`,
`next_mach20`)

On Coda, these files may be found in
`coda/project/coda/alpha/{include,lib,bin}`.

On Andrew, these files are located in
`/afs/andrew/scs/cs/15-612{include,lib,bin}`.

iopen () is a system call created at Carnegie Mellon.  Put an empty dummy
function if your C library doesn't have it.

## Debugging

The following external RPC2 variables may be set for debugging:

`RPC2_DebugLevel`

:   Values of 0, 1, 10 and 100 are meaningful.  Initial value is 0.

`RPC2_Perror`

:   Set to 1 to see Unix error messages on stderr.  Initial value is 1.

`RPC2_Trace`

:   Set to 1 to enable tracing. 0 turns off tracing.  Initial value is 0.  Also
    see description of `RPC2_InitTrace` and `RPC2_DumpTrace` calls.
