# rvm_set_range

## NAME

rvm_set_range - specify a range of memory to be modified in a transaction

## SYNOPSIS

```c
#include "rvm.h"

rvm_return_t rvm_set_range (tid, addr, length)
rvm_tid_t    *tid;      /* pointer to transaction identifier */
char         *addr;     /* virtual address of first byte of range */
rvm_length_t length;    /* length of range in bytes */
```

## DESCRIPTION

rvm_set_range specifies a range of virtual memory that will be modified
within the transaction. The range must be entirely contained within a
mapped region, otherwise RVM_ENOT_MAPPED will be returned.

The range is associated with the transaction identified by tid.
After setting the range, modifications to the memory can be performed
using normal assignment statements. RVM will capture both old and new
values for this range when the transaction commits.

If the transaction is begun in restore mode, the old value of each byte
in the range is saved so that it can be restored if the transaction aborts.
If the transaction is begun in no_restore mode, only the new values are
recorded at commit time; no old values are preserved.

The specified address must be within a mapped region. The length may
extend beyond the end of the region; RVM will clip it to the region's bounds.

## DIAGNOSTICS

- **RVM_SUCCESS**: success
- **RVM_ETID**: invalid transaction identifier pointer
- **RVM_ENOT_MAPPED**: specified range not within a mapped region
- **RVM_ELOG**: no log file has been specified
- **RVM_EIO**: I/O error during logging
- **RVM_ENO_MEMORY**: heap exhausted
- **RVM_EPORTABILITY**: internal portability problem, see system maintainer

## SEE ALSO

[rvm_begin_transaction](rvm_begin_transaction.3.md), [rvm_modify_bytes](rvm_modify_bytes.3.md)

## AUTHOR

Hank Mashburn
