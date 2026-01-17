# rvm_load_segment

## NAME

rvm_load_segment - load a segment using its load map header

## SYNOPSIS

```c
#include "rvm_segment.h"

typedef struct {
    rvm_offset_t        offset;
    rvm_length_t        length;
    char                *vmaddr;
} rvm_region_def_t;

rvm_return_t rvm_load_segment (DevName, DevLength, options,
                               nregions, regions)
char             *DevName;  /* name of heap segment */
rvm_offset_t     DevLength; /* length of heap raw partition */
rvm_options_t    *options;  /* optional pointer to an options record */
unsigned long    *nregions;  /* number of region descriptors  */
rvm_region_def_t *regions[]; /* pointer to array of region descriptors  */
```

## DESCRIPTION

`rvm_load_segment` is used to automatically map the regions of a
segment prepared with `rvm_create_segment`.  The name of the file or
partition for the segment is specified by `DevName`, and the
length of the partition is specified in `DevLength`.  If the heap 
is in a file, `DevLength` should be zero.

`rvm_load_segment` will first to load the
array of `rvm_region_def_t` descriptors contained in the segment
header.  If the segment header is not recognizable, the error
`RVM_ESEGMENT_HDR` will be returned.  A version identifier will also
be checked, and if the version of the segment library that created the
header does not match the version of the library linked with the
application, the error `RVM_EVERSION_SKEW` will be returned.
Otherwise, the address and length of the region descriptors array will
be returned in regions and `nregions`, respectively.
The header array must be deallocated by the application.

After the header is verified, each of the regions will be mapped via
`rvm_map`.  Allocation of virtual memory is done by
`rvm_load_segment`, so the application must not attempt allocation, or
the error `RVM_ERANGE` will result.
Regions whose virtual memory address is non-zero will be
automatically mapped to that address.  Regions with zero virtual
memory addresses 
will have space allocated by RVM, and the allocated address will be
returned in the `vmaddr` field of the descriptor.  
After successful completion of
`rvm_load_segment`, all data specified by the header is addressable
to the application.

Regions are not allowed to overlap in either the segment or virtual
memory.  If this condition is not met, the error codes
`RVM_EOVERLAP` or `RVM_EVM_OVERLAP` will be returned, as
appropriate.

Since it is an initialization function, `rvm_load_segment`
performs no internal synchronization, so if there 
is a possibility of concurrent access to the segment,
the application must do appropriate serialization.

An optional RVM options descriptor can be passed to
`rvm_load_segment` if no log has been previously declared.
RVM must be initialized before `rvm_load_segment` is called.

## DIAGNOSTICS

- **RVM_SUCCESS** success
- **RVM_ESEGMENT_HDR** segment header invalid
- **RVM_EVERSION_SKEW** segment header built with different version of segment loader library
- **RVM_EOVERLAP** segment regions overlap
- **RVM_EVM_OVERLAP** segment regions overlap in virtual memory
- **RVM_EOPTIONS** invalid options record or pointer
- **RVM_ENO_MEMORY** heap exhausted, or buffer cannot be allocated
- **RVM_ELOG** no log file has been specified or invalid file name
- **RVM_EINIT** RVM not initialized

## SEE ALSO

[rvm_create_segment](rvm_create_segment.3.md), [rvm_map](rvm_map.3.md), [rds_load_heap](rds_load_heap.3.md)

## AUTHOR

David C. Steere
