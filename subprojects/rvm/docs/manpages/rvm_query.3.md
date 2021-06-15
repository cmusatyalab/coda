# rvm_query

## NAME

rvm_query - query RVM options and internal state

## SYNOPSIS

```c
#include "rvm.h"

rvm_return_t rvm_query (options)
rvm_options_t *options;   /* pointer to options record */
```

## DESCRIPTION

rvm_query returns the current state of RVM options and resource usage.

The options parameter must be a valid rvm_options_t record, allocated with
rvm_malloc_options or initialized with rvm_init_options. Upon return,
the record will be filled with current values:

- **log_dev**: The name of the log file (must point to a buffer of MAXPATHLEN+1)
- **truncate**: Current truncation threshold percentage
- **recovery_buf_len**: Length of truncation buffer in bytes
- **flush_buf_len**: Length of flush buffer in bytes
- **max_read_len**: Maximum disk transfer length (Mach only)
- **log_empty**: Set to true if log is empty
- **pager**: Name of external pager file
- **n_uncommit**: Number of uncommitted transactions outstanding
- **tid_array**: Pointer to array of transaction identifiers (if n_uncommit > 0)
- **flags**: Current option flags

## DIAGNOSTICS

- **RVM_SUCCESS**: success
- **RVM_EOPTIONS**: invalid options record or pointer
- **RVM_ENO_MEMORY**: heap exhausted (for tid_array allocation)
- **RVM_EPORTABILITY**: internal portability problem, see system maintainer

## SEE ALSO

[rvm_set_options](rvm_set_options.3.md), [rvm_initialize](rvm_initialize.3.md)

## AUTHOR

Hank Mashburn
