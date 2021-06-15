# rvm_terminate

## NAME

rvm_terminate - clean termination of RVM runtime library

## SYNOPSIS

```c
#include "rvm.h"

rvm_return_t rvm_terminate ()
```

## DESCRIPTION

rvm_terminate cleanly shuts down the RVM library.
It will detect and return an error code if any transactions are
found to be uncommitted at the time of shutdown.

As part of orderly termination, the application designer should
consider forcing a log truncation with rvm_truncate. This will avoid
the inconvenience of truncation delay during the first mapping the
next time the application is started.

After RVM has been terminated, it cannot be re-initialized by calling
rvm_initialize.

## DIAGNOSTICS

- **RVM_SUCCESS**: success
- **RVM_ETRAN**: uncommitted transactions exist (see rvm_query)
- **RVM_EIO**: I/O error during termination
- **RVM_EPORTABILITY**: internal portability problem, see system maintainer

## SEE ALSO

[rvm_initialize](rvm_initialize.3.md), [rvm_truncate](rvm_truncate.3.md), [rvm_query](rvm_query.3.md)

## AUTHOR

Hank Mashburn
