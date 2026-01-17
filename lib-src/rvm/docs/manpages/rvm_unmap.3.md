# rvm_unmap

## NAME

rvm_unmap - unmap a previously mapped segment region from virtual memory

## SYNOPSIS

```c
#include "rvm.h"

rvm_return_t rvm_unmap (region)
rvm_region_t *region;   /* pointer to region descriptor */
```

## DESCRIPTION

rvm_unmap unmaps the specified region of a segment from the virtual
memory of the calling process. After unmapping, the region is no longer
accessible through the virtual memory address.

Before unmapping, RVM checks that there are no uncommitted transactions
in the region. If uncommitted transactions exist, RVM_EUNCOMMIT will be
returned and the region will not be unmapped.

The region descriptor must have been previously initialized by a call
to rvm_map. After unmapping, the virtual memory address in the region
descriptor is no longer valid.

## DIAGNOSTICS

- **RVM_SUCCESS**: success
- **RVM_EUNCOMMIT**: uncommitted transactions exist in region (see rvm_query)
- **RVM_ENOT_MAPPED**: region not currently mapped
- **RVM_EIO**: I/O error during unmapping
- **RVM_EPORTABILITY**: internal portability problem, see system maintainer

## SEE ALSO

[rvm_map](rvm_map.3.md), [rvm_query](rvm_query.3.md)

## AUTHOR

Hank Mashburn
