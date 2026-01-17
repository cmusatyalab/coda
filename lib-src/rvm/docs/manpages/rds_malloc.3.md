# rds_malloc

## NAME

rds_malloc - allocate recoverable heap storage

## SYNOPSIS

```c
#include "rds.h"

int rds_malloc (size, tid, err);
unsigned long size;  /* length of allocation request */
rvm_tid_t     *tid;  /* optional pointer to transaction identifier */
int           *err;  /* pointer to error return location */
```

## DESCRIPTION

`rds_malloc` allocates from the recoverable heap a block large
enough to hold the request, specified in bytes by the `size` parameter.
The address of the allocated block is returned as type
`int` and must be cast to the desired type.
If the request cannot be satisfied, zero is returned, and the `err`
parameter is set to the appropriate error code.

Because allocation actions in the recoverable heap must be done
via transactions, `rds_malloc` offers two choices for the transaction.
In the first case, `rds_malloc` can be instructed to use an existing
transaction begun in `restore` mode, by passing the address of a
valid `rvm_tid_t` record in the `tid` parameter.
This avoids extra transaction start and commit
overhead and provides automatic deallocation if the transaction must
later abort.
However, no context swap can be permitted between allocation and
transaction commit or abort since the 
modified, but uncommitted, free lists are visible to other RDS actions.

If context swaps cannot be prohibited, or the available transaction was
started in `no_restore` mode, the `tid` parameter should be
set to null, instructing `rds_malloc` to start an internal transaction.
This transaction will be committed in `no_flush` mode if the
allocation is made, and aborted otherwise.
In this case, an explicit `rds_free` must be done if the allocating
transaction later aborts.

## DIAGNOSTICS

- **SUCCESS**: success (error will be 0)
- **ENO_ROOM**: RDS heap exhausted
- **ECORRUPT**: heap corruption detected
- **EHEAP_INIT**: RDS not initialized
- **RVM error codes**: various errors from RVM subsystem

## SEE ALSO

[rds_free](rds_free.3.md)

## AUTHOR

David C. Steere

## BUGS

The internal synchronization is not valid with pre-emptive threads.
