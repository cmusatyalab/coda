# rvm_end_transaction

## NAME

rvm_end_transaction - commit a transaction

## SYNOPSIS

```c
#include "rvm.h"

rvm_return_t rvm_end_transaction (tid, mode)
rvm_tid_t    *tid;      /* pointer to transaction identifier */
rvm_mode_t   mode;      /* transaction commit mode: flush or no_flush */
```

## DESCRIPTION

rvm_end_transaction commits the specified transaction.
All modifications made within the transaction become permanent.

The mode parameter specifies whether the transaction records should be
immediately flushed to stable storage:

- **flush**: The log is flushed to disk before returning, guaranteeing
  that all changes are permanent. This mode ensures durability but has
  higher latency.
- **no_flush**: The transaction records are queued for later flushing,
  allowing for faster commits. There is a risk of losing committed changes
  if the system crashes before the log is flushed.

After committing, the transaction identifier remains valid and can be reused
for a new transaction.

## DIAGNOSTICS

- **RVM_SUCCESS**: success
- **RVM_ETID**: invalid transaction identifier pointer
- **RVM_EMODE**: illegal commit mode
- **RVM_ELOG**: no log file has been specified or log error
- **RVM_EIO**: I/O error during commit
- **RVM_ENO_MEMORY**: heap exhausted
- **RVM_EPORTABILITY**: internal portability problem, see system maintainer

## SEE ALSO

[rvm_begin_transaction](rvm_begin_transaction.3.md), [rvm_abort_transaction](rvm_abort_transaction.3.md), [rvm_flush](rvm_flush.3.md)

## AUTHOR

Hank Mashburn
