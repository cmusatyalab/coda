# rds_init_heap

## NAME

rds_init_heap - initialize a recoverable heap in an existing segment

## SYNOPSIS

```c
#include "rds.h"

int rds_init_heap (base, length, chunkSize, nlists, tid, err)
char          *base;     /* base address of heap */
unsigned long length;    /* length of heap */
unsigned long chunkSize; /* size of allocation unit */
unsigned long nlists;    /* number of allocation lists */
rvm_tid_t     *tid;      /* pointer to transaction identifier */
int           *err;      /* pointer to error return location */
```

## DESCRIPTION

`rds_init_heap` initializes a recoverable heap in the previously
mapped memory region specified by the address `base` and `length`
parameters.
Allocation requests will be rounded up to an integral number of
allocation units `chunkSize`, which is specified in bytes, and must
be an integral multiple of `sizeof (char *)`, and be at least
`RDS_MIN_CHUNK_SIZE`.

For rapid allocation, RDS maintains separate allocation lists for
blocks of different sizes.  The sizes are integrals of `chunkSize`,
beginning with 1 and extending to the number of lists specified by
`nlists`, which must be at least `RDS_MIN_FREE_LISTS`.  These
lists are initially empty and the entire heap is
placed on the large block list as a single block.  `rds_malloc` will
split this block as required.

If a number of blocks of specific sizes are required by the
application, the function `rds_prealloc` is provided for efficient
pre-allocation.  It should be called immediately after initializing
the heap.

An active transaction must be specified in the `tid` parameter; RDS
will not create an internal transaction for this function.
This transaction must be committed by the application for the
initialization to be permanent.

Since this function is called only to initialize the heap, RDS assumes
there is no concurrent access and does no internal locking.

## DIAGNOSTICS

- **SUCCESS**: success
- **ENO_ROOM**: heap length not long enough for heap structures
- **ECORRUPT**: initialization failed due to corruption
- **EBAD_CHUNK_SIZE**: the chunk size is not large enough or is not a multiple of sizeof(char *)
- **EBAD_LIST_LENGTH**: the number of free lists is not at least RDS_MIN_FREE_LISTS
- **RVM error codes**: various errors from RVM subsystem

## SEE ALSO

[rds_zap_heap](rds_zap_heap.3.md), [rds_prealloc](rds_prealloc.3.md)

## AUTHOR

David C. Steere
