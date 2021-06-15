# rvm_begin_transaction

## NAME

rvm_begin_transaction - begin a transaction

## SYNOPSIS

```c
#include "rvm.h"

rvm_return_t rvm_begin_transaction (tid,mode)

rvm_tid_t    *tid;    /* pointer to transaction identifier */
rvm_mode_t   mode;    /* transaction begin mode */
```

## DESCRIPTION

rvm_begin_transaction initiates a transaction.
A unique transaction identifier is assigned and returned in the record
specified by tid parameter. If the pointer does not reference a valid
rvm_tid_t record, the RVM_ETID exception code is returned.
If the transaction cannot be started because of internal memory limitations,
RVM_ENO_MEMORY is returned.

rvm_begin_transaction recognizes two transaction modes: restore and no_restore.
Transactions begun with restore create old value records so that the state
of virtual memory can be restored if the transaction aborts.
If mode is not a legal option, RVM_EMODE is returned.

The no_restore mode transactions do not create old value records.
Therefore, an abort does not restore the state of virtual memory.
They are offered as an additional performance enhancement for situations where
there is no possibility that the application will issue an abort for the transaction.

Once a transaction is begun, modifications to mapped regions can be made with
rvm_modify_bytes, or by the program in a range specified by rvm_set_range.
The only restriction on modifications is that the modified regions must be mapped.

Transactions are ended with rvm_end_transaction, or rvm_abort_transaction,
as appropriate.

## DIAGNOSTICS

- **RVM_SUCCESS**: success
- **RVM_ETID**: invalid transaction identifier pointer
- **RVM_EMODE**: illegal transaction mode
- **RVM_ENO_MEMORY**: heap exhausted
- **RVM_ELOG**: no log file has been specified
- **RVM_EINIT**: RVM not initialized

## SEE ALSO

[rvm_set_range](rvm_set_range.3.md), [rvm_modify_bytes](rvm_modify_bytes.3.md),
[rvm_end_transaction](rvm_end_transaction.3.md), [rvm_abort_transaction](rvm_abort_transaction.3.md)

## AUTHOR

Hank Mashburn
