# rds_fake_free

## NAME

rds_fake_free - record intention to free a block

## SYNOPSIS

```c
#include "rds.h"

typedef struct intlist {
    unsigned long           size;
    unsigned long           count;
    char                    **table;
} intentionList_t;

int rds_fake_free (addr, list)
char             *attr;    /* block to free */
intentionList_t  *list;     /* intention list header */

int rds_do_free(addr, list);
intentionList_t *list; /* pointer to intention list */
rvm_mode_t      mode;  /* transaction commit mode */
```

## DESCRIPTION

In cases where the application must free recoverable storage without
knowing whether the transaction will later abort, `rds_free` should
not be used since the internal free list lock is not held until commit
or abort.

To handle these cases, RDS allows the creation of an *intention list*
 for the blocks to be freed.  At the point where the block would
logically be freed, `rds_fake_free` is called to place the block on
the intention list.

To actually free the blocks on the intention list, `rds_do_free` is
called.  This must be done immediately before calling
`rvm_end_transaction` for the deallocating transaction, and must not
be done if
`rvm_abort_transaction` is called.  Also, no context swaps can be
allowed between the calls on `rds_do_free` and `rvm_end_transaction`.

The intention list header (type `intentionList_t`) must be provided
by the application.  The `table`
field must be null on the first call to `rds_fake_free`.  RDS will
allocate space as necessary for the blocks to be freed.
`rds_do_free` will deallocate the `table` vector.  The application
is responsible for deallocation of `table` if the transaction is
aborted.  Deallocation of the `intentionList_t` itself is always the
responsibility  of the application.

RDS does not internally synchronize access to the intention list.
If more than one thread is involved in the transaction,
synchronization of access to the intention list is the responsibility
of the application.

The block to be deallocated is passed in the `addr` parameter to
`rds_fake_free` .  This block must not already be free, or the `EFREED_TWICE`
error code will be returned.  The `ECORRUPT` error return is given by
`rds_fake_free` if the block guards are damaged.  `rds_do_free` returns this
code if internal damage to the free lists is found.

These functions return error codes as the
value of the function rather than in a parameter.

## DIAGNOSTICS

- **SUCCESS**: success
- **EFREED_TWICE**: block is already free
- **ECORRUPT**: heap corruption detected
- **EBAD_ARGS**: block is not on 4 byte boundary
- **EHEAP_INIT**: RDS not initialized
- **RVM error codes**: various errors from RVM subsystem

## SEE ALSO

[rds_free](rds_free.3.md), [rds_malloc](rds_malloc.3.md)

## AUTHOR

David C. Steere
