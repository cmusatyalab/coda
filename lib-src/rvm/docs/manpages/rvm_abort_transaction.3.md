# rvm_abort_transaction

## NAME

rvm_abort_transaction - abort a transaction

## SYNOPSIS

```c
#include "rvm.h"

rvm_return_t rvm_abort_transaction (tid)
rvm_tid_t    *tid;      /* pointer to transaction identifier */
```

## DESCRIPTION

rvm_abort_transaction aborts the specified transaction.
If the transaction was begun in restore mode, the old value records are used
to restore all affected memory regions to their state before the transaction began.
If the transaction was begun in no_restore mode, no restoration occurs.

All modifications made within the transaction are discarded. The transaction
identifier remains valid and can be reused for a new transaction.

## DIAGNOSTICS

- **RVM_SUCCESS**: success
- **RVM_ETID**: invalid transaction identifier pointer
- **RVM_ELOG**: no log file has been specified
- **RVM_EIO**: I/O error during abort
- **RVM_EPORTABILITY**: internal portability problem, see system maintainer

## SEE ALSO

[rvm_begin_transaction](rvm_begin_transaction.3.md), [rvm_end_transaction](rvm_end_transaction.3.md)

## AUTHOR

Hank Mashburn
