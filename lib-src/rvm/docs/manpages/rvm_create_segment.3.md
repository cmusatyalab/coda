# rvm_create_segment

## NAME

rvm_create_segment - create a segment with load map header

## SYNOPSIS

```c
#include "seg.h"

typedef struct {
    rvm_offset_t        offset;
    rvm_length_t        length;
    char                *vmaddr;
} rvm_region_def_t;

rvm_return_t rvm_create_segment (DevName, DevLength, options,
                                 nregions, regions)

char             *DevName;  /* name of heap segment */
rvm_offset_t     DevLength; /* length of heap raw partition */
rvm_options_t    *options;  /* optional pointer to an options record */
unsigned long    nregions;  /* number of region descriptors */
rvm_region_def_t *regions;  /* pointer to array of region descriptors */
```

## DESCRIPTION

`rvm_create_segment` is used to initialize the segment header with
the region load map.
The name of the file or partition
for the segment is specified by `DevName`, and the
length of the partition is specified in `DevLength`.  If the segment
is represented by a file, `DevLength` should be zero.  Also, the
file must exist and be at least one page long.

The parameter `region_defs` must point to an array of
`rvm_region_def_t` descriptors, with
one descriptor for each region to be loaded by
`rvm_load_segment`.  
The descriptor array must be allocated and deallocated by application,
and must be as long as the number of descriptors specified by
`nregions`.  The maximum number of regions that can be described in
the header is limited by the page size of the machine, which is
available as the macro `RVM_PAGE_SIZE`.  If the number of regions
requested by the parameter `nregions` causes the header to exceed
page size, the error `RVM_ENO_MEMORY` will be returned.

Each `rvm_region_def_t` descriptor requires the regions virtual
memory address and its length.  The virtual memory address where the
region will be 
loaded must be page-aligned.  It can be specified as zero and, if so, will
cause the loader to allocate virtual memory for the region.
Regions containing pointers must
specify an address to guarantee that they are loaded in the same place
each time.  
The region length must be an integral multiple of the page size, or
the error code `RVM_ERANGE` will be returned.

Regions are not allowed to overlap in virtual
memory.  If this condition is not met, the error code
`RVM_EVM_OVERLAP` will be returned.

`rvm_create_segment` assigns space in the segment to the header and
regions contiguously, beginning with the header at byte zero and with
length equal to the page size.  The regions are allocated next, in the
order of their occurrence in the `region_defs` array.  The offset is
returned in the `offset` field of each descriptor.

The virtual memory specified in the descriptors does not
have to be addressable when calling `rvm_create_segment`.
Also, `rvm_create_segment` does not map or alter the applications
data in the segment; only the first page of the segment for the header
is affected.

Since it is an initialization function, `rvm_create_segment`
performs no internal synchronization, so if there 
is a possibility of concurrent access to the segment,
the application must do appropriate serialization.

An optional RVM options descriptor can be passed to
`rvm_create_segment` if no log has been previously declared.
RVM must be initialized before `rvm_create_segment` is called.

## DIAGNOSTICS

- **RVM_SUCCESS**: success
- **RVM_ERANGE**: a region length is not page sized
- **RVM_EVM_OVERLAP**: region mappings oberlap in virtual mapping
- **RVM_ENO_MEMORY**: heap exhausted, or header cannot be allocated
- **RVM_EOPTIONS**: invalid options record or pointer
- **RVM_ELOG**: no log file has been specified
- **RVM_EINIT**: RVM not initialized

## SEE ALSO

[rvm_load_segment](rvm_load_segment.3.md), [rvm_map](rvm_map.3.md), [rds_zap_heap](rds_zap_heap.3.md),
[rds_load_heap](rds_load_heap.3.md), [rdsinit](rdsinit.1.md)

## AUTHOR

David C. Steere
