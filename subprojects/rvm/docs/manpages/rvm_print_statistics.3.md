# rvm_print_statistics

## NAME

rvm_print_statistics - format and print statistics

## SYNOPSIS

```c
#include "rvm_statistics.h"

typedef struct
    {
    /* see rvm_statistics.h for current fields */
    }
rvm_statistics_t;

rvm_return_t     rvm_print_statistics (statistics,out_stream);
rvm_statistics_t *statistics; /* pointer to a statistics record */
FILE             *out_stream; /* pointer to stdio stream */
```

## DESCRIPTION

rvm_print_statistics formats an rvm_statistics_t record previously filled in by
rvm_statistics. The formatted output is sent to the specified stream, which must
first have been opened with fopen.

For detailed specification of the statistics currently collected, consult the header
file rvm_statistics.h, which is also included in the RVM manual.

This function can also be invoked from rvmutl with the statistics command.

## DIAGNOSTICS

- **RVM_SUCCESS**: success
- **RVM_ESTATISTICS**: Invalid statistics record or pointer
- **RVM_EIO**: I/O or kernel error, errno has code number
- **RVM_ELOG**: no log file has been specified
- **RVM_EINIT**: RVM not initialized

## SEE ALSO

[rvm_statistics](rvm_statistics.3.md), [rvmutl](rvmutl.1.md)

## AUTHOR

Hank Mashburn
