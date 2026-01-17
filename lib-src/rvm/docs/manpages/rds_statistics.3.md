# rds_statistics

## NAME

rds_statistics - collect RDS performance statistics

## SYNOPSIS

```c
#include "rds.h"

int           rds_get_stats (stats);
rds_stats_t   *stats; /* pointer to RDS statistics record */

int           rds_clear_stats (err);
int           *err;   /* pointer to error return location */

int           rds_print_stats();
```

## DESCRIPTION

RDS maintains simple statistics about the recoverable heap and the
allocations performed.  The statistics area is kept in the recoverable
heap region and is initialized when the heap is initialized.  

The statistics can be read by `rds_get_stats` into an `rds_stats_t`
record for analysis by the application, or printed on `stdout` by
`rds_print_stats`.
`rds_get_stats (stats)` and `rds_print_stats ()` return error codes as the
value of the function rather than in a parameter.

After heap initialization, the statistics area can be cleared by
calling `rds_clear_stats`.
`rds_clear_stats` simply zeroes the statistics area via an internal
transaction.  The space 
allocated and free counts will not be accurate if it is called after
initializing the RDS heap.  Note that `rds_clear_stats` requires an
error return code as a parameter, while the others return the error
status as the function value.

RDS performs no internal synchronization for these calls, so if there
is a possibility of concurrent access to the statistics, the accesses
must be serialized by the application.

## DIAGNOSTICS

- **SUCCESS**: success
- **EHEAP_INIT**: RDS not initialized
- **EBAD_ARGS**: null RDS statistics record pointer

## AUTHOR

David C. Steere
