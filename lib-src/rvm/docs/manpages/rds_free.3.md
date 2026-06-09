# rds_free

## NAME

rds_free - free recoverable heap storage

## SYNOPSIS

```c
#include "rds.h"

int rds_free (addr, tid, err)
char          *addr; /* address of block to be freed */
rvm_tid_t     *tid;  /* optional pointer to transaction identifier */
int           *err;  /* pointer to error return location */
```

## DESCRIPTION

`rds_free` deallocates recoverable storage allocated by `rds_malloc`.
The block to be deallocated is passed in the `addr` parameter.  This
block must not already be free, or the `EFREED_TWICE` error
code will be returned in `err`.

Because deallocation actions in the recoverable heap must be done via
transactions, `rds_free` offers two choices for the transaction.
In the first case, `rds_free` can be instructed to use an existing
transaction by passing the address of a valid `rvm_tid_t` record in
the `tid` parameter.  This avoids extra transaction start and commit
overhead and provides automatic reallocation if the transaction must
later abort.

If the available transaction was started in `no_restore` mode, the
`tid` parameter should be set to null, instructing `rds_free` to
start an internal transaction.
This transaction will be committed in `no_flush` mode if the
allocation is made, and aborted otherwise.

In neither case can context swaps be permitted between deallocation and
transaction commit or abort since the modified, but uncommitted, free lists are
visible to other RDS actions.
If this condition cannot be met, or if there is a
possibility that the transaction will abort, the functions
`rds_fake_free` and `rds_do_free` must be used.

## DIAGNOSTICS

- **SUCCESS**: success
- **EFREED_TWICE**: block is already free
- **ECORRUPT**: heap corruption detected
- **EBAD_ARGS**: block is not on 4 byte boundary
- **EHEAP_INIT**: RDS not initialized
- **RVM error codes**: various errors from RVM subsystem

## SEE ALSO

[rds_malloc](rds_malloc.3.md), [rds_fake_free](rds_fake_free.3.md)

## AUTHOR

David C. Steere

## BUGS

The internal synchronization is not valid with pre-emptive threads.
