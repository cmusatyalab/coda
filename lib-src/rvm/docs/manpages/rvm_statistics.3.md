# rvm_statistics

## NAME

rvm_statistics - query collected statistics

## SYNOPSIS

```c
#include "rvm_statistics.h"

typedef struct
    {
    /* see rvm_statistics.h for current fields */
    }
rvm_statistics_t;

rvm_return_t     rvm_statistics (version, statistics);
char             *version;    /* Statistics version string */
rvm_statistics_t *statistics; /* pointer to a statistics record */

rvm_return_t     RVM_STATISTICS (statistics);
rvm_statistics_t *statistics; /* pointer to a statistics record */
```

## DESCRIPTION

rvm_statistics copies the statistics collected by RVM to a record provided by
the application. The contents of the record can then be analyzed by the
application, or formatted by rvm_print_statistics.

Note that the header rvm_statistics.h must be included to use the statistics
facility. These declarations are not included in the standard RVM library header
since they are expected to change as RVM is further developed.
Because of the expectation of change, a version parameter is used to detect
version skews between the statistics record format of the application and the
RVM library. The string constant RVM_STATISTICS_VERSION from rvm_statistics.h
is passed to rvm_statistics, and compared with the value the library was compiled
with. If they are different, RVM_ESTAT_VERSION_SKEW is returned and no data is
copied to the statistics record.

The macro RVM_STATISTICS will automatically pass the version string to the function.

The statistics parameter specifies the record to which the statistics will be copied.
This record must be of type rvm_statistics_t and must have been created by
rvm_malloc_statistics or initialized with rvm_init_statistics. If the record is
not properly formatted by either of the above functions, the error RVM_ESTATISTICS
is returned.

For detailed specification of the statistics currently collected, consult the
header file rvm_statistics.h which is also included in the RVM manual.

## DIAGNOSTICS

- **RVM_SUCCESS**: success
- **RVM_ESTAT_VERSION_SKEW**: Statistics version skew
- **RVM_ESTATISTICS**: Invalid statistics record or pointer
- **RVM_ELOG**: no log file has been specified
- **RVM_EINIT**: RVM not initialized

## SEE ALSO

[rvm_print_statistics](rvm_print_statistics.3.md)

## AUTHOR

Hank Mashburn
