# rvm_flush

## NAME

rvm_flush - force all pending transaction records to be written to the log

## SYNOPSIS

```c
#include "rvm.h"

rvm_return_t rvm_flush ()
```

## DESCRIPTION

rvm_flush forces all previously committed transactions that have not yet been
flushed to the log to be written to stable storage. This ensures durability
of those changes.

If the log is already empty, the function returns immediately with RVM_SUCCESS.
If other threads are also attempting to flush simultaneously, operations are
serialized in the order received.

Flushing is typically automatic when transactions commit in flush mode,
but this function allows explicit control over when flushing occurs.
This can be useful for applications that want to batch multiple transactions
and then force them all to stable storage at once.

## DIAGNOSTICS

- **RVM_SUCCESS**: success (log may already have been empty)
- **RVM_ELOG**: log file not open or error during flush
- **RVM_EIO**: I/O error during flush operation
- **RVM_EPORTABILITY**: internal portability problem, see system maintainer

## SEE ALSO

[rvm_end_transaction](rvm_end_transaction.3.md), [rvm_truncate](rvm_truncate.3.md)

## AUTHOR

Hank Mashburn
