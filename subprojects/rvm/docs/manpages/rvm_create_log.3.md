# rvm_create_log

## NAME

rvm_create_log - create a log file

## SYNOPSIS

```c
#include "rvm.h"

rvm_return_t rvm_create_log (path, size)
char         *path;     /* pathname of log file */
rvm_length_t size;       /* size of log in bytes */
```

## DESCRIPTION

rvm_create_log creates a new log file with the specified name and size.
The file must not already exist; if it does, RVM_EEXISTS will be returned.

This function provides programmatic creation of log files. Typically, logs are
created using the rvmutl utility program, which can also handle raw disk partitions.

The size parameter specifies the amount of space in the log for transaction records.
Once a log is created, its size cannot be changed without reinitialization.

## DIAGNOSTICS

- **RVM_SUCCESS**: success
- **RVM_EEXISTS**: file already exists
- **RVM_ELOG**: error opening or creating log file
- **RVM_EIO**: I/O error during creation
- **RVM_EPORTABILITY**: internal portability problem, see system maintainer

## SEE ALSO

[rvmutl](rvmutl.1.md), [rvm_initialize](rvm_initialize.3.md)

## AUTHOR

Hank Mashburn
