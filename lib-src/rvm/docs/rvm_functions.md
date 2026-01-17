# RVM Library Functions

In this and the next two chapters, functions of the RVM runtime library
will be briefly presented in groups, and then the actual man pages of the
functions will be presented.

## Initialization, Options, and Mapping Functions

There are five functions for this groups:

- [rvm_initialize](manpages/rvm_initialize.3.md): Initialization of RVM runtime library.
- [rvm_set_options](manpages/rvm_set_options.3.md): Changes RVM options after initialization.
- [rvm_terminate](manpages/rvm_terminate.3.md): Clean termination of RVM runtime library.
- [rvm_map](manpages/rvm_map.3.md): Mapping all or part of data segment to virtual memory.
- [rvm_unmap](manpages/rvm_unmap.3.md): Ummapping previouslly mapped segment.

## Transaction Functions

Transactions are used to change recoverable storage.  A single
transaction can modify any number of mapped regions.  Addresses used
in modifications to recoverable storage are virtual addresses and only
virtual memory is modified directly.  RVM will perform the translation
into segment offsets based on the mapped region descriptors.  The
modifications become permanent only when a transaction commits.

To initiate a transaction, a transaction identifier record (rvm_tid_t)
is created and presented to rvm_start_transaction which will give the
transaction a unique identifier.  A pointer to the record becomes a
handle that must be used for all subsequent operations on the
transaction.  The record can be created with the rvm_malloc_tid
function (see section [on RVM structures](rvm_library.md#allocation-functions).

The transaction identifier record has two time value fields that mark
the approximate time when the transaction was begun.  Because the Unix
kernel call gettimeofday underlying the timestamps is defined in
microseconds, but many machine clocks do not have that fine
resolution, the value can be no more precise than the machines clock.
Also, RVM may increment the lowest order bits to generate a unique
name without having to perform a kernel call.  The value can still be
presented to the time formatting routines available to extract a
printable time.  The fields, both integers, are contained in the
`struct timeval` field `uname`:

- `tv_sec`: Seconds since Jan. 1, 1970.
- `tv_usec`: Microseconds since last second "tick."

RVM offers several options to control logging in transactions.
The intent in all cases is to take advantage of special situations that may
be known to the application and can be used to optimize the timing of,
or eliminate, log operations.
If a transaction cannot abort, using no_restore transactions will
enhance performance.

The options supported are:

- **restore**: Create old and new value log records for modifications; restore virtual memory after an abort.
- **no_restore**: Do not create old value log records; virtual memory not restored after an abort.
- **flush**: Flush log on commit.
- **no_flush**: Do not flush log on commit.

Note that options can be specified at transaction start and commit.
The restore, and no_restore options are valid only at the
beginning of a transaction.
The flush and no_flush options are valid only at transaction
end (commit).
At either transaction beginning or end, only one option can be specified.

- [rvm_begin_transaction](manpages/rvm_begin_transaction.3.md)
- [rvm_set_range](manpages/rvm_set_range.3.md)
- [rvm_modify_bytes](manpages/rvm_modify_bytes.3.md)
- [rvm_abort_transaction](manpages/rvm_abort_transaction.3.md)
- [rvm_end_transaction](manpages/rvm_end_transaction.3.md)

## Log Control Operations

### Log Flush and Truncation

If an application is scheduling log operations, it can explicitly
invoke the flush and truncate operations with the rvm_flush and
rvm_truncate functions.
Both of these functions are considered *slow* operations since they
require disk operations, and will lock out other RVM functions
requiring disk access, particularly rvm_end_transaction in flush mode.
If invoked by more than one thread, these operations will be
serialized in the order received by RVM.

RVM will recognize when an operation is not needed because the log
is empty, and will return immediately.

### Log Initialization

The function rvm_create_log can be used to create log files,
*provided* that the file does not already exist.
This requirement is made to prevent applications that automatically
create a log file on startup from destroying the log records that are
necessary to recover from a crash.

Either files or raw disk partitions can be initialized as logs with the
utility rvmutl. (Raw disk partitions can only be initialized by
rvmutl).  Details are found in [rvmutl](rvmutl.md).

- [rvm_flush](manpages/rvm_flush.3.md)
- [rvm_truncate](manpages/rvm_truncate.3.md)
- [rvm_create_log](manpages/rvm_create_log.3.md)

## Query and Statistics Functions

RVM allows the application to query the settings of options and some
internal state with `rvm_query`.  Access to the statistics
automatically collected by RVM is provided by `rvm_statistics`.

The query and statistics functions require that the application
allocate records for the data to be returned.  In general, the
application is responsible for deallocation of these records, although
they can be used in multiple calls to their respective functions.
As with other records types in RVM, the `rvm_options_t` and
`rvm_statistics_t` records must be allocated or initialized with the
provided type-specific functions.

Because `rvm_query` must return two variable length items, some
special allocation conventions must be observed.
If the name of the log file is to be returned, the caller must
allocate a buffer of `MAXPATHLEN+1` characters and place a pointer
to it in the `log_dev` field.
The file name will be copied into the buffer.
The buffer will not be deallocated by RVM.
The `log_dev` field should be null if the name of the log is not to
be returned.

The tid_array vector is always returned if uncommitted
transactions are present.  Since the length isnt known before the
call, `rvm_query` must do the allocation.  This results in the
following deallocation conventions:
for option records allocated with `rvm_malloc_options`, the array
should not be deallocated by the application since this will be done by
`rvm_free_options`.
However, if the options record is allocated on the stack, the array
must be explicitly deallocated by the application, or space will be
lost.

Deallocation of the array in statically allocated options records is
optional: repeated use of the record will cause the array to be
reallocated as necessary by `rvm_query` with no explicit action by
the application.  If you do deallocate the array, you *must* null the
pointer to avoid a possible internal error due the dangling pointer.

No special deallocation conventions are required for
`rvm_statistics_t` records.

- [rvm_query](manpages/rvm_query.3.md)
- [rvm_statistics](manpages/rvm_statistics.3.md)
- [rvm_print_statistics](manpages/rvm_print_statistics.3.md)
