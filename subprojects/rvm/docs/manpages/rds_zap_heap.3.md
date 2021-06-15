# rds_zap_heap

## NAME

rds_zap_heap - create a new segment with initialized heap

## SYNOPSIS

```c
#include "rds.h"

int rds_zap_heap (DevName, DevLength, startAddr, staticLength,
                  heapLength, nlists, chunkSize, err)
char          *DevName;  /* name of heap segment */
rvm_length_t  DevLength; /* length of heap raw partition */
char          *startAddr;/* base address of heap */
rvm_length_t  staticLength; /* length of static region */
rvm_length_t  heapLength;/* length of heap region */
unsigned long nlist;     /* number of allocation lists */
unsigned long chunkSize; /* size of allocation unit */
int           *err;      /* pointer to error return location */
```

## DESCRIPTION

`rds_zap_heap` prepares a recoverable heap for loading by
`rds_load_heap`.  Two regions are created in the heap segment named
by `DevName`, one for the applications statically allocated region
and one for the recoverable heap.
The lengths of the regions, specified by `staticLength` and
`heapLength`, must be integrals of the system page size.  The heap
region is allocated at `*startAddr` in virtual memory, which must be
on a page
boundary.  This address is the permanent address of the heap.  The
static region starts at `*startAddr` + `heapLength` and is also
permanent.  One additional region is created at the beginning of the
segment for the segment header, and is one page long.  This region is
only temporarily mapped by `rds_load_heap`.  The heap region follows
the header in the segment, and the static region follows the heap.

If the segment is represented by a raw partition, the maximum usable
length of the partition  must be specified in `DevLength`.  For
files, this length should be zero.

The region of virtual memory for the heap and static regions should
not be allocated by the application.  `rds_zap_heap` uses the RVM
segment utilities to build the segment header and map the regions.
When `rds_zap_heap` is complete, the memory will be mapped.

`rds_zap_heap` uses `rvm_create_segment` to build the segment, and
then calls `rds_init_heap` to initialize the free
lists as specified by the `nlists` and `chunkSize` parameters.
`chunkSize` is specified in bytes, and must
be an integral multiple of `sizeof (char *)`, and be at least
`RDS_MIN_CHUNK_SIZE`.  The number of free lists must be at least
`RDS_MIN_FREE_LISTS`.
The transaction required by `rds_init_heap` is created and
terminated internally.

If a number of blocks of specific sizes are required by the
application, the function `rds_prealloc` is provided for efficient
pre-allocation.  It should be called immediately after initializing
the heap.

The utility `rdsinit` is usually used to create recoverable heap
segments.  It prompts for the parameters and calls `rds_zap_heap`.

Since this function is called only to initialize the heap, RDS assumes
there is no concurrent access and does no internal locking.

## DIAGNOSTICS

- **SUCCESS**: success
- **ENO_ROOM**: heap length not long enough for header
- **EBAD_CHUNK_SIZE**: the chunk size is not large enough or is not a multiple of `sizeof(char *)`
- **EBAD_LIST_LENGTH**: the number of free lists is not at least `RDS_MIN_FREE_LISTS`
- **RVM error codes**: various errors from RVM subsystem

## SEE ALSO

[rds_init_heap](rds_init_heap.3.md), [rds_load_heap](rds_load_heap.3.md), [rvm_create_segment](rvm_create_segment.3.md),
[rvm_load_segment](rvm_load_segment.3.md), [rdsinit](rdsinit.1.md), [rds_prealloc](rds_prealloc.3.md)

## AUTHOR

David C. Steere
