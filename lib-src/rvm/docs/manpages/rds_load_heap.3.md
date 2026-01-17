# rds_load_heap

## NAME

rds_load_heap - load an existing heap using segment loader

## SYNOPSIS

```c
#include "rds.h"

int rds_load_heap (DevName, DevLength, staticAddr, err);
char         *DevName;  /* name of heap segment */
rvm_length_t DevLength; /* length of heap raw partition */
char         **staticAddr; /* address of static region (out)*/
int          *err;      /* pointer to error return location */

int rds_start_heap (staticAddr, err);
char         **staticAddr; /* address of static region (out)*/
int          *err;      /* pointer to error return location */
```

## DESCRIPTION

`rds_load_heap` provides a convenient method of mapping the heap
segment and initializing RDS.  The name of the file or partition
containing the recoverable heap is specified by `DevName`, and the
length of the partition is specified in `DevLength`.  If the heap 
is in a file, `DevLength` should be zero.

`rds_load_heap` calls `rvm_load_segment` to map the heap.  This
requires that the heap file or partition be created with
`rds_zap_heap` or `rvm_create_segment`, which is most conveniently
done with the `rdsinit` utility.
After the heap is mapped, the address of the static region is returned
in `staticAddr`.  `rds_start_heap` is automatically called to
initialize RDS.

If the actual mapping of the heap and static regions is done by the
application, `rds_start_heap` should be used to initialize RDS.  In this
case, the address of the static region must be specified to RDS in the
`staticAddr` parameter.

With either function, the version of RDS that the heap was built with
is compared with the version of the currently linked RDS library.  
If there is a mismatch, the error code `EHEAP_VERSION_SKEW` is returned.

Since these functions are called only to initialize the application,
RDS assumes there is no concurrent access and does no internal locking.

## DIAGNOSTICS

- **SUCCESS**: success (error will be 0)
- **EHEAP_VERSION_SKEW**: RDS heap and library have different versions
- **RVM error codes**: various errors from RVM subsystem

## SEE ALSO

[rvm_load_segment](rvm_load_segment.3.md), [rvm_map](rvm_map.3.md), [rdsinit](rdsinit.1.md)

## AUTHOR

David C. Steere
