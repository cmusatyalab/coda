# rvm_modify_bytes

## NAME

rvm_modify_bytes - specify a range of memory to be modified in a transaction (alternative interface)

## SYNOPSIS

```c
#include "rvm.h"

rvm_return_t rvm_modify_bytes (tid, dest, src, length)
rvm_tid_t    *tid;      /* pointer to transaction identifier */
char         *dest;     /* virtual address of first byte of destination range */
char         *src;      /* virtual address of source data */
rvm_length_t length;    /* length of range in bytes */
```

## DESCRIPTION

rvm_modify_bytes is an alternative interface for specifying ranges of memory
to be modified within a transaction. Unlike rvm_set_range which only records
the range, rvm_modify_bytes copies the source data into an internal buffer.

The destination range must be entirely contained within a mapped region,
otherwise RVM_ENOT_MAPPED will be returned.

rvm_modify_bytes is particularly useful when the entire range is to be replaced.
It avoids the need for separate calls to specify the range and then perform
the actual modifications.

The behavior of old value preservation depends on whether the transaction
was begun in restore or no_restore mode, exactly as with rvm_set_range.

## DIAGNOSTICS

- **RVM_SUCCESS**: success
- **RVM_ETID**: invalid transaction identifier pointer
- **RVM_ENOT_MAPPED**: specified range not within a mapped region
- **RVM_ELOG**: no log file has been specified
- **RVM_EIO**: I/O error during logging
- **RVM_ENO_MEMORY**: heap exhausted
- **RVM_EPORTABILITY**: internal portability problem, see system maintainer

## SEE ALSO

[rvm_begin_transaction](rvm_begin_transaction.3.md), [rvm_set_range](rvm_set_range.3.md)

## AUTHOR

Hank Mashburn
