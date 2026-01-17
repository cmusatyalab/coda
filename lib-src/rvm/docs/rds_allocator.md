# RDS, A Dynamic Heap Allocator for RVM

*Designed and programmed by David C. Steere*

## Introduction to RDS

The Recoverable Dynamic Storage (RDS) library provides a heap similar to that of the
C library, although with persistence, for RVM segments. Additional provisions for
convenient initialization and reloading of the heap after shutdown or crash are included.

RDS uses the Quick Fit method[@weinstock88] for allocation. In this method, separate free
lists are maintained for a specified number of sizes. Most programs tend to repeatedly
allocate a small number of distinct sized blocks. Quick Fit avoids overhead by retaining
those block sizes as long as possible in the expectation that they will be re-allocated
when freed. Coalescing of adjacent free blocks is deferred until a request cannot be satisfied.

The free list sizes are integer multiples of the *chunk size*, which is specified when
the recoverable heap is created and cannot be modified thereafter except by
re-initializing the heap. The chunk size must be an integral multiple of
`sizeof (char *)`, and be at least `RDS_MIN_CHUNK_SIZE`. The free lists are maintained
in sizes of one chunk to *n* chunks, where *n* is the maximum number of free lists.
The number of lists is also specified at heap creation and cannot be modified thereafter.

When an allocation request is made, the requested size is rounded up to an integral
multiple of the chunk size. The free list for that size is checked first, and if not
empty, the first block is returned. Otherwise, the next larger free list is checked,
and if not empty, the first block is selected, the requested size split off, and the
remainder placed on the appropriate free list. If the free list is empty, the next is
searched. If no specific-sized free list can allocate the block, the request is taken
from the single list of larger blocks by splitting the first block on the list.
The remainder is returned to the appropriate free list. Sizes larger than *n* times
the chunksize are also allocated from the large blocks list on a first-fit basis.
This list is always created in addition to the fixed size lists.

When a block is freed, it is simply placed on the appropriate free list with no attempt
coalesce it with adjacent free blocks. Coalescing is deferred until an allocation request
cannot be satisfied.

The allocation and deallocation functions, rds_malloc and rds_free, resemble malloc and
free, although they are modified for transactions. To help prevent fragmentation, RDS also
provides for pre-allocation of pools of block sizes that the application will repeatedly
allocate with the function rds_prealloc.

The function rds_load_heap provides a convenient way to use the RVM segment loader to load
the persistent heap and a statically allocated region. rds_load_heap will make the necessary
RVM mapping calls and initialize RDS. When it completes, the allocations persistent data is
ready for use.

Initialization of an RDS heap is done with the function rds_init_heap. A second function,
rds_zap_heap, which calls rds_init_heap and rvm_create_segment, can be used to prepare a
segment for loading via rds_load_heap. However, heap initialization is usually done with
the utility rdsinit, which will prompt for the necessary parameters to call rds_zap_heap.

To assist in detecting buffer overruns and other memory clobbers, RDS puts special *guard*
constants at the beginning and end of each block. These are not within the space allocated
to satisfy an allocation request and will never be addressed by a correctly operating
application. The rounding up of allocation request sizes to integral multiples of the chunk
size automatically includes space for the guard constants.

If memory damage is detected by the corruption of one of the guards when a block is
deallocated, or when damage to a free list is detected during allocation, the error code
ECORRUPT is returned. If this error is detected during deallocation, the block is not put
on a free list, but is left unmodified for analysis of how the overwrite occurred.

RDS generally returns error indications in a parameter whose address must be specified in
the call. However, a few functions return the error code as their value. RDS error codes are
all defined as negative integers in rds.h. Because RDS must perform its actions via transactions,
the RVM error codes can also be returned in the error code parameter or as function values.
These are defined in `rvm.h`, and are all positive values. Success is always indicated by the
code SUCCESS, which is defined to be zero.

The header file giving all necessary definitions for RDS is included in Appendix
[C Declaration of RVM](rvm_headers.md).

## Transactions and RDS

All functions in RDS use RVM transactions. Some, particularly rds_malloc and rds_free,
must interact with the applications transactions, requiring some care from the caller.
RDS does not hold its lock until after commit so that greater concurrency can possibly be
achieved in Coda. This is not generally recommended because it permits uncommitted state to
be used by other than the calling transaction. See the notes on good programming practice with
RVM, section [Transactions in Critical Sections](rvm_design.md#transactions-in-critical-sections).

Since the lock is not held until commit, some additional complexity must be introduced to
insure that allocation and deallocation actions are independent. There are two cases that
must be recognized: if the caller guarantees that no context swaps among application threads
will occur before transaction commit, and if such swaps must be allowed.

If context swaps are avoided, the application can supply the transaction to be used by RDS
in the allocation/deallocation calls and be sure that if the transaction must later be aborted,
all will be restored correctly. The transaction must be started in restore mode, which is
required for all RDS calls. The transaction will not be committed or aborted by RDS.

If context swaps must be allowed (frequently the case), a null transaction parameter must be
specified. This will force an internal transaction to be created by RDS. This transaction
will be committed by RDS before the function returns. In this case, if `rds_malloc` is called
and the application transaction requesting the allocation must abort, the block must be
explicitly deallocated by a call to rds_free. Otherwise, recoverable heap space will be lost.

When the application deallocates blocks in a transaction that could later abort and context
swaps are possible, two special functions are provided, rds_fake_free and rds_do_free.
These functions manage an *intention list* that records the intended deallocations, but defers
the actual deallocation until the application transaction is ready to commit. If the
transaction aborts, the intention list is discarded. The application must call rds_fake_free
at the point where the block would logically be freed, and then call rds_do_free immediately
before committing the application transaction. The application must provide the intention
list header, of type `intentionList_t`; RDS will manage the actual list.

Note that these methods are safe *only* in a coroutine thread environment. If pre-emptive
threads, or a multiprocessor, are required, RDS cannot safely be used.

- [**rds_malloc**](manpages/rds_malloc.3.md)
- [**rds_free**](manpages/rds_free.3.md)
- [**rds_fake_free**](manpages/rds_fake_free.3.md)
- [**rds_init_heap**](manpages/rds_init_heap.3.md)
- [**rds_load_heap**](manpages/rds_load_heap.3.md)
- [**rds_zap_heap**](manpages/rds_zap_heap.3.md)
- [**rds_prealloc**](manpages/rds_prealloc.3.md)
- [**rds_statistics**](manpages/rds_statistics.3.md)
- [**rdsinit**](manpages/rdsinit.1.md)
