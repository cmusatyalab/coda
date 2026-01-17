# RVM Library Specifications

RVM is a library of functions to be linked with the application and is executed
within the address space and control thread of the calling process. For single
process applications, this design eliminates RPC calls, although extensions to
RVM for distributed transactions will require them.

The functions implementing the core of RVM are included in one user level library.
Two versions of the library are necessary: one for Unix, and one for Mach.
The Unix version will run under Mach, but no Mach-specific features, such as
external pager support or C Threads, will be available. The interface to C is
specified in one header file that is used with both versions.

Auxiliary libraries of useful functions are also available. These can extend the
functionality of RVM, or provide convenience features useful to a broad range of
applications. Two of these, a utility for loading regions of segments, and an
allocator for segments, are documented in Chapter [RVM Segment Loader](rvm_segment_loader.md)
and Chapter [RDS, A Dynamic Heap Allocator](rds_allocator.md).

All RVM functions return success/failure codes of type `rvm_return_t`.
The code, `RVM_SUCCESS`, is returned by all functions to report successful
execution. If an exception is encountered in executing a library function,
an appropriate code is returned. These codes are specified with the functions.
If the `RVM_EIO` code is returned, the specific error causing it will be left in
errno. RVM functions check their parameters at entry, so unless otherwise
specified in the function descriptions, a function returning an error code will
have had no effect.

Library functions can also return information via output parameters.
When this method is used, descriptions of the information returned will be
specified with the function.

## RVM Files and Structures

A single header file, rvm.h (included in Appendix [C Declaration of RVM](rvm_headers.md)),
provides the basic declarations for using RVM. rvm.h recognizes if it has been
previously included in a compilation and will not create multiple definitions.
In the Carnegie-Mellon programming environment, the enumeration typedef bool,
with constants true and false, and the typeless constants TRUE and FALSE are often
defined with the include file stdlib.h (or libc.h) at CMU. For convenience,
rvm.h also supplies these, but will not attempt to redefine them if stdlib.h has
been included.

Other header files are used for optional features of RVM. These include headers
for statistics collection and printing, an allocator, and a segment loading utility.

In building the application, the choice of Unix or Mach libraries must be made.
If the application is using threads, Mach must be used, and the application must
also link the C Threads library and the safe version of the C library (see @cooper88),
p. 13). The Unix library will run on Mach/Unix, but will be strictly synchronous
so threads must not be used in the application.

The necessary files are:

- header files: `~/include/rvm.h`, `~/include/rvm_statistics.h`
- library: `~/lib/librvm.a`
- log utility: `~/bin/rvmutl`

Each structure used in RVM has a self-identifier as its first field (struct_id).
These are used by the library functions to validate the record type and are also
valuable in debugging since the type, regardless of casts (intentional or
otherwise), will be available. The type code will automatically be set by
type-specific allocation and initialization routines provided by RVM.
The struct_id field must not be modified.

The print names for all RVM codes and typedefs can be obtained with functions
similar to strerror. These functions return a pointer to a character array
containing the name. These arrays are statically allocated and the pointers must
not be deallocated with free; no deallocation is necessary. The functions are:

```c
char *rvm_return (code)/* function return codes */
rvm_return_t code;

char *rvm_mode (mode)/* transaction modes */
rvm_mode_t mode;

char *rvm_type (struct_id)/* typedef/structure  names */
rvm_struct_id_t id;
```

## Allocation Functions

The structures used in RVM have allocation, initialization, copy, and
deallocation functions defined for them. The allocation and deallocation
functions maintain a cache of heap allocated records within RVM.
All type-specific initialization and finalization is automatically done,
as needed, by these routines. Initialization for non-heap allocated records is
also provided. At present there are three types allocated: rvm_options_t,
rvm_region_t, and rvm_tid_t. These represent the options descriptor,
discussed below, the mapped regions descriptor, and the transaction identifier.

These functions take their names from the type they support. For example,
the allocation function for rvm_options_t is `rvm_malloc_options`, which
requires no parameters and returns a pointer to a new options record.
Similarly, the deallocation function for a transaction identifier (rvm_tid_t) is
`rvm_free_tid (tid)`, which takes a single parameter, tid, which is a pointer to
a transaction identifier record. Strings used in RVM records are always copies
of pointers to the previously existing character arrays, so no deallocation
is necessary.

The initialization and copy functions are named `rvm_init_~` and `rvm_copy_~`
respectively, and take a single parameter: the pointer to the record affected.
The copy functions return a pointer to the created record. The `rvm_init_~`
functions must be applied to statically, globally, or automatically allocated
records before RVM will recognize them. The `rvm_free_~` functions will ignore
such non-heap allocated records. A field, from_heap, is present in all RVM
structures so that the space freeing functions can detect non-heap allocated
records. It is set by the allocation functions, and must not be altered.

The function specifications are:

```c
/********/
/* utility routines for rvm_options_t (option descriptor, or opt. desptr.) */
/********/
rvm_options_t *opt;                   /* pointer to option descriptor      */

rvm_options_t *rvm_malloc_options();  /* allocate, initialize opt. desptr. */
                                       /* return pointer                    */

rvm_free_options(opt);                /* free the opt. desptr. pointed to  */
                                       /* by opt                            */

rvm_init_options(opt);                /* initialize non-heap allocated     */
                                       /* opt. desptr.                      */

rvm_options_t *rvm_copy_options(opt); /* copy opt. desptr. pointed to by   */
                                       /* opt to a newly allocated opt.     */
                                       /* desptr., return pointer           */
/********/
/* utility routines for rvm_region_t (region descriptor, or reg. desptr.)  */
/********/
rvm_region_t *reg;                    /* pointer to region descriptor      */

rvm_region_t *rvm_malloc_region();    /* allocate, initialize reg. desptr. */
                                       /* return pointer                    */

rvm_free_region(reg);                 /* free the reg. desptr. pointed to  */
                                       /* by reg                            */

rvm_init_region(reg);                 /* initialize non-heap allocated     */
                                       /* reg. desptr.                      */

rvm_region_t *rvm_copy_region(reg);   /* copy reg. desptr. pointed to by   */
                                       /* reg to a newly allocated reg.     */
                                       /* desptr., return pointer           */
/********/
/* utility routines for rvm_tid_t (transaction identifer, or tid)          */
/********/
rvm_tid_t *tid;                       /* pointer to transaction identifier */

rvm_tid_t *rvm_malloc_tid();          /* allocate, initialize tid          */
                                       /* return pointer                    */

rvm_free_tid(tid);        	      /* free the tid pointed to by tid    */

rvm_init_tid(tid);        	      /* initialize non-heap allocated tid */

rvm_tid_t *rvm_copy_tid(tid);         /* copy tid pointed to by opt        */
                                       /* to a newly allocated tid          */
                                       /* return pointer                    */
```

Implementing RVM as a library limits freedom to change the program interface
because change would require code changes in all applications. To alleviate this,
RVM uses *option records* to specify parameters for certain features.
New features will be implemented by adding new fields to the records so program
changes will be limited to those applications needing the new features.
Others will need only to recompile and link, an automated process for most systems.

The option records can specify global settings, particularly for the log,
and also options for individual recoverable storage segments.
Some features may require setting at initialization time, and can not be
changed later. Details of each option are presented as the features are discussed.

## Initialization, Options and Mapping

RVM requires initialization by the client application. In addition to initializing
its internal structures, RVM will check for version skews between the version of
rvm.h used to build the application and the version of librvm.a linked.

At initialization, the name of the log file is specified and other options can
also be specified. Most options (except the log file) can be changed after RVM has
been initialized, and there is provision to query the state of all options.

To insure an orderly shutdown of RVM, a termination function is also provided.
It will detect and return an error code if any transactions are found to be
uncommitted at the time of shutdown. As part of orderly termination, the
application designer should consider forcing a log truncation. This will avoid
the inconvenience of truncation delay during the first mapping the next time the
application is started.

### RVM Options Descriptor

When a region is mapped, certain options can be specified in an `rvm_options_t`
record created with `rvm_malloc_options`, which takes no parameters.
Option records can also be used with `rvm_query` to discover the state of RVM
options and resource usage. At present the following mapping option fields are
defined:

- **log_dev**: Name of the log file. Required on initialization and cannot be
  changed thereafter. Returned on query.
- **truncate**: Log truncation threshold: percentage of the log file capacity.
  When this much of the log is full, truncation will be initiated.
  Zero inhibits the truncation thread. Actual percentage of log used at time of
  returned; default: 50%, specified by the constant TRUNCATE.
- **recovery_buf_len**: Length of truncation buffer in bytes.
- **flush_buf_len**: Length of flush buffer in bytes.
- **max_read_len**: Maximum disk transfer length (Mach only).
- **log_empty**: State returned on query (bool).
- **pager**: Name of external pager file (char array).
- **n_uncommit**: Number of uncommitted transactions returned on query.
- **tid_array**: Pointer to an array of transaction identifiers of length
  n_uncommit returned on query.
- **flags**: Bit vector of option flags.

Unless otherwise specified in rvm.h, all fields are unsigned long integers.

The log_dev field is a pointer to a character array that contains the Unix file
name of the log. This must be specified on initialization and is returned on
query. The log file cannot be changed after initialization.

The truncate option, specified as a percentage of log file capacity, indicates
the threshold for automatic log truncation. The default is 50%, and the actual
threshold is returned on query.

If the value of truncate is non-zero, RVM creates an internal thread to do
truncations asynchronously; a zero value inhibits automatic truncations if a
thread was already created. When the automatic truncations are inhibited,
truncations are initiated by the application with `rvm_truncate`, or by remapping
a previously mapped and modified region.

If automatic truncation is inhibited and the log file overflows, a truncation is
performed by the thread encountering the overflow. This is an emergency measure
and applications should not allow it to happen since all transactions will be
blocked until the truncation completes. However, when running without thread
support, this is the only method of initiating truncation.

The recovery_buf_len field specifies the length, in bytes, of the buffer used by
truncation. It is set to a default value of 256 Kbytes, which is generally sufficient.

The flush_buf_len field specifies the length, in bytes, of the buffer used to
flush log records on raw partitions. It is not used for file-based logs.
The default length is 256 Kbytes.

The max_read_len field specifies the length, in bytes, of the maximum read
transfer that RVM will perform. This is a Mach-only feature, and is used to limit
the amount of kernel buffering required when large segments are mapped.
This field is ignored on Unix systems. The default of 512 Kbytes seems to work well.

log_empty will be set by `rvm_query` if the log is empty. This is to help
applications determine when it is safe to transfer access control to a segment
between processes.

The pager option is used to specify a Mach external pager to be associated with
the segment being mapped. This option may only be set at initialization time.
If the string provided does not correspond to an existing file or device,
`RVM_EOPTIONS` will be returned. RVM will then return to the state before any
mapping was attempted.

The fields n_uncommit and tid_array are returned by `rvm_query` to allow an
application to discover uncommitted transactions. This is necessary if either
`rvm_unmap` or `rvm_terminate` return an error code because such transactions
are outstanding. n_uncommit is set to the number of uncommitted transactions
outstanding in a region, or in the entire segment, if no region is specified.
If n_uncommit is not zero, tid_array will point to an array of length n_uncommit
of transaction identifiers.

The flags field is a bit vector used to specify other options to RVM.
At present, the transaction optimization options RVM_COALESCE_RANGES and
RVM_COALESCE_TRANS can be specified. Both are specified with RVM_ALL_OPTIMIZATIONS.
The options in use are returned by `rvm_query`.

### RVM Region Descriptor and Segment Mapping

Regions of recoverable storage are mapped into virtual memory for manipulation
by a process. Mapped regions are described by the typedef rvm_region_t.
This structure (actually pointers to its instances) is used as a handle to specify
regions in mapping and transaction function calls. The fields of the structure are:

- **data_dev**: Name of the segment file.
- **dev_length**: 64 bit segment maximum length.
- **offset**: 64 bit region offset in segment.
- **vmaddr**: Virtual memory base address of mapped region.
- **length**: Length, in bytes, of region.
- **no_copy**: Do not copy segment data for mapped region if true.

The file name and virtual memory address are pointers to character arrays.

The dev_length field is of type rvm_offset_t, a structure specifying the 64 bit
disk offset. It specifies the maximum length of a segment that may be mapped,
and permits enforcement of a resource limit on a segment. Optional for segments
represented by files, it is required for disk partitions since there is no easy
way to discover the partition size at run-time.

The offset field is of type rvm_offset_t, and is the offset in the segment of the
first byte of the region.

The length field is of type rvm_length_t. This numeric type is guaranteed to
always be of sufficient size to represent the maximum region length meaningful
in the host machine address space. Functions and macros to manipulate length
and offset types are provided and are described in the next section.

The no_copy field offers a performance enhancement when the data would be entirely
replaced by the application. When the option is chosen by setting the field to
true, the segment region is associated with the specified virtual memory region,
but no data from the segment is copied into memory. This option is available with
or without the external pager.

Programs prepare for RVM function calls by creating an rvm_region_t record with
the allocator `rvm_malloc_region`. Statically or globally allocated records must
be initialized with `rvm_init_region`, which requires a pointer to the record.
All field values are entered directly into the records.

When the region records are used, the file name is always required, but other
fields may sometimes not be used. Certain fields are also used to return
information. The descriptions of the functions will specify what is required and
what is returned. After a region is mapped, the descriptor is used for all
functions referencing the region.

## Statistics Collection

RVM collects various statistics about its performance and the characteristics of
the applications transaction behavior. All statistics are collected for both
current and cumulative periods. The current period begins at RVM initialization
time and continues until the next truncation. At truncation time, the current
values are summed with the cumulative values. The current fields are then reset.
Reseting the cumulative values can be done only by reinitializing the log,
so the cumulative history is maintained from the time the log is initialized.

To read the statistics, a `rvm_statistics_t` record is allocated with
`rvm_malloc_statistics`, and passed to the `rvm_statistics` function.
The current and cumulative values are copied into the record and it is returned.

As RVM is developed, the statistics are expected to change. Consequently, the
definition of the `rvm_statistics_t` record that is used to present the values to
the application is not included in the standard RVM library header. A secondary
header file, `rvm_statistics.h`, is used so that the statistics version can
change independently of the primary program interface. If the application program
makes minimal direct use of the fields in the `rvm_statistics_t` record,
the anticipated changes should cause little reprogramming inconvenience.

To further insulate the application program from changes in the statistics,
a printing function `rvm_print_statistics`, is also included.
If statistics are simply captured and printed, the details of the statistics
record are completely hidden. The statistics can also be printed with the
`rvmutl` log utility.

For detailed specification of the statistics currently collected,
consult the header file `rvm_statistics.h`, which printed in Appendix
[C Declaration of RVM](rvm_headers.md).

## Address, Length, and Offset Arithmetic

The `rvm_length_t` and `rvm_offset_t` types are intended to shield applications
from machine-dependent representations of large values. Using basic C types such as
unsigned long for region lengths may not work on machines with virtual address
spaces greater than 2^32. Type long may continue to be defined as a 32-bit
quantity in some C compilers so rvm_length_t will always be defined as an integer
type wide enough to hold the largest virtual memory region possible on the host
machine.

Similarly, disk memories greater than 2^32 already exist. The rvm_offset_t will
be guaranteed to provide 64-bit integer representation for segment offsets.
On most present machines, this is implemented as two 32-bit fields, but on
machines with 64-bit integer formats, rvm_offset_t may be defined as a single
field. Applications are strongly discouraged from directly using the internal
representations of either rvm_length_t or rvm_offset_t. Such use may create
portability problems.

The following functions and macros are provided for manipulating rvm_length_ts
and virtual memory addresses:

```c
RVM_ADD_LENGTH_TO_ADDR (vmaddr,length) /* add length to address, */
  char          *vmaddr;  /* returning result as value of macro */
  rvm_length_t  length;   /* with type char *  */

RVM_SUB_LENGTH_FROM_ADDR (vmaddr,length) /* subtract length from address, */
  char          *vmaddr;  /* returning result as value of macro */
  rvm_length_t  length;   /* with type char *   */
```

The following macros are provided for manipulating, comparing, and converting
offsets:

```c
RVM_ADD_OFFSETS (x,y)  /* add y to x, */
  rvm_offset_t  x;     /* return offset result */
  rvm_offset_t  y;

RVM_SUB_OFFSETS (x,y)  /* subtract y from x, */
  rvm_offset_t  x;     /* return offset result */
  rvm_offset_t  y;

RVM_ADD_LENGTH_TO_OFFSET (x,y)  /* add length y to offset x, */
  rvm_offset_t  x;     /* return offset result */
  rvm_length_t  y;

RVM_SUB_LENGTH_FROM_OFFSET (x,y)  /* subtract length y from offset x, */
  rvm_offset_t  x;     /* return offset result */
  rvm_length_t  y;

RVM_MK_OFFSET (x,y)    /* make an offset from two lengths  x, y, */
                       /* return offset result */
  rvm_length_t  x;     /* high order bits */
  rvm_length_t  y;     /* low order bits */

RVM_OFFSET_INITIALIZER (x,y)  /* compile-time initializer for offsets */
  rvm_length_t  x;     /* high order bits */
  rvm_length_t  y;     /* low order bits */

RVM_ZERO_OFFSET (x)    /* zero all 64 bits of x */
  rvm_offset_t  x;

RVM_OFFSET_TO_LENGTH (x)  /* return low-order bits of offset x as length */
  rvm_offset_t  x;

RVM_OFFSET_HIGH_BITS_TO_LENGTH (x)  /* return the high-order bits offset x as length */
  rvm_offset_t  x;

RVM_LENGTH_TO_OFFSET (x)  /* construct offset from length x */
  rvm_length_t  x;
```

The comparison macros return 1 if the comparison is true; 0 otherwise:

```c
RVM_OFFSET_~(x,y)   /* comparators, return 1 if comparison is true */
  rvm_offset_t  x;  /* operators (~): LSS, LEQ, GTR, GEQ, EQL */
  rvm_offset_t  y;

RVM_OFFSET_EQL_ZERO (x)  /* return 1 if offset x is zero */
  rvm_offset_t  x;
```

For convenience in mapping, the following macros provide integral, page-sized
conversions:

```c
RVM_PAGE_SIZE       /* return page size as rvm_length_t */

RVM_ROUND_LENGTH_UP_TO_PAGE_SIZE (x)  /* return length x rounded up */
  rvm_length_t  x;  /* to next integral page-sized length */

RVM_ROUND_LENGTH_DOWN_TO_PAGE_SIZE (x)  /* return length x truncated */
  rvm_length_t  x;  /* to integral page-sized length */

RVM_ROUND_ADDR_UP_TO_PAGE_SIZE (x)  /* return address x rounded up */
  char  *x;         /* to next integral page size */

RVM_ROUND_ADDR_DOWN_TO_PAGE_SIZE (x)  /* return address x truncated */
  char  *x;         /* to integral page size */

RVM_ROUND_OFFSET_UP_TO_PAGE_SIZE (x)  /* return offset x rounded up */
  rvm_offset_t  x;  /* to next integral page-sized offset */

RVM_ROUND_OFFSET_DOWN_TO_PAGE_SIZE (x)  /* return offset x truncated */
  rvm_offset_t  x;  /* to integral page-sized offset */
```
