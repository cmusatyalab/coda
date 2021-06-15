# rvm_map

## NAME

rvm_map - map all or part of a data segment to virtual memory

## SYNOPSIS

```c
#include "rvm.h"

rvm_return_t rvm_map (region, options)
rvm_region_t *region;   /* pointer to region descriptor */
rvm_options_t *options; /* pointer to options descriptor */
```

## DESCRIPTION

rvm_map establishes a mapping between a virtual memory region and a
segment. The segment is mapped into the address space of the calling
process at the base address specified in the region descriptor.

The region descriptor contains fields that specify the segment file name,
the segment length, the offset in the segment, the virtual memory address
and length of the region, and an option to avoid copying data from the
segment when mapping (no_copy). The options descriptor contains fields
that specify external pager information and transaction optimization flags.

If a virtual memory address is specified as zero (NULL), RVM will select
an available region of virtual memory. The selected base address will be
returned in the region descriptor. All other fields must be filled in,
but the length field in the region descriptor will be returned as the
actual length mapped, which may differ from the requested length if it is
not page aligned.

If the log is not empty when rvm_map is called, RVM will automatically
perform log truncation before mapping. This ensures that the segment data
represents the most recent committed image.

## DIAGNOSTICS

- **RVM_SUCCESS**: success
- **RVM_EOPTIONS**: invalid options or external pager file
- **RVM_EOVERLAP**: region overlaps with an existing mapped region
- **RVM_ENO_MEMORY**: insufficient virtual address space or heap exhausted
- **RVM_ENOT_MAPPED**: specified range not within a mapped region
- **RVM_EINIT**: RVM not initialized
- **RVM_ELOG**: log file not open or error during log truncation
- **RVM_EIO**: I/O error on segment file or log file
- **RVM_EPORTABILITY**: internal portability problem, see system maintainer

## SEE ALSO

[rvm_unmap](rvm_unmap.3.md), [rvm_initialize](rvm_initialize.3.md)

## AUTHOR

Hank Mashburn
