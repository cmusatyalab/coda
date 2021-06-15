# rvm_truncate

## NAME

rvm_truncate - truncate the log by applying committed changes to data segments

## SYNOPSIS

```c
#include "rvm.h"

rvm_return_t rvm_truncate ()
```

## DESCRIPTION

rvm_truncate applies all committed transaction records in the log to their
corresponding data segments and reclaims the log space used by those records.

This operation is necessary for continued operation of RVM. As transactions
commit, the log fills up. Periodically, the log must be truncated to free
space for new transaction records.

RVM can be configured to automatically truncate the log when it reaches a
certain threshold percentage full (specified in the options during initialization).
Alternatively, applications can call rvm_truncate explicitly to control when
truncation occurs.

Truncation involves reading all unapplied transaction records from the log,
applying them to the appropriate segments, and then reclaiming the log space.
This is a potentially expensive operation, but it ensures that the log does not
grow unboundedly.

## DIAGNOSTICS

- **RVM_SUCCESS**: success
- **RVM_ELOG**: log file not open or error during truncation
- **RVM_EIO**: I/O error during truncation
- **RVM_ENO_MEMORY**: heap exhausted during truncation
- **RVM_EPORTABILITY**: internal portability problem, see system maintainer

## SEE ALSO

[rvm_flush](rvm_flush.3.md)

## AUTHOR

Hank Mashburn
