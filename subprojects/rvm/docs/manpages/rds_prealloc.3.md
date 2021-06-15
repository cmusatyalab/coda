# rds_prealloc

## NAME

rds_prealloc - pre-allocate blocks for repeated use

## SYNOPSIS

```c
#include "rds.h"

int rds_prealloc (size, nblocks, tid, err)
unsigned long size;    /* size of allocation request */
unsigned long nblocks; /* length of allocation request */
rvm_tid_t     *tid;    /* pointer to transaction identifer */
int           *err;    /* pointer to error return location */
```

## DESCRIPTION

`rds_prealloc` assists applications in minimizing heap fragmentation
by pre-allocating pools of blocks in sizes that the application is
known to use in relatively large numbers.
`rds_prealloc` is best called immediately after the heap is
initialized, although it can be called anytime.
When pools of several block sizes are pre-allocated, they should be
allocated in increasing block size order so that the larger blocks are
not split to make the smaller.

The `size` parameter specifies the size, in bytes, of the blocks to
be pre-allocated, and `nblocks` controls the number of blocks
pre-allocated.

Because allocation actions in the recoverable heap must be done via
transactions, `rds_prealloc` offers two choices for the transaction.
In the first case, `rds_prealloc` can be instructed to use an existing
transaction begun in `restore` mode, by passing the address of a
valid `rvm_tid_t` record in the `tid` parameter.
This avoids extra transaction start and commit
overhead and provides automatic deallocation if the transaction must
later abort.
However, no context swap can be permitted between allocation and
transaction commit or abort since the uncommitted state is visible to
other allocations.

If context swaps cannot be prohibited, or the available transaction was
started in `no_restore` mode, the `tid` parameter should be
set to null, instructing `rds_prealloc` to start an internal transaction.
This transaction will be committed in `no_flush` mode if the
allocation is made, and aborted otherwise.
In this case, if the allocating
transaction later aborts, the pre-allocated blocks remain allocated.

## DIAGNOSTICS

- **SUCCESS**: success (error will be 0, pointer array returned)
- **ENO_ROOM**: RDS heap exhausted
- **ECORRUPT**: RDS heap damaged
- **EHEAP_INIT**: RDS not initialized
- **RVM error codes**: various errors from RVM subsystem

## SEE ALSO

[rds_malloc](rds_malloc.3.md), [rds_init_heap](rds_init_heap.3.md), [rds_zap_heap](rds_zap_heap.3.md)

## AUTHOR

David C. Steere

## BUGS

The internal synchronization is not valid with pre-emptive threads.
