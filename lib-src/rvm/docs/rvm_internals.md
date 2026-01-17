# RVM Internals

*By Yui Wah LEE*

## Overview of RVM internal structure

RVM is designed to be a *run-time library* to be linked with an *external application*.
It allows the application to be benefited from *transactional* support. In particular, it
allows operations on virtual memory to be *atomic* and *persistent*, provided that the
application declares its intention through the RVM application programming interface.

XXX overview figure

*Overview of RVM structure shows the three important components of RVM:
**virtual memory**, **data segment** and **log**.*

Virtual memory is where the application manipulates its data. RVM allows the application
to declare *regions* of virtual memory to be associated with regions in external data
segments. The data segments are the persistent backing store of the virtual memory regions:
latest committed image of the virtual memory region has a copy in the data segments.
Since data segment is on stable storage (disk), it is persistent across program crashes.
Should an application crash, it can always find those memory regions from the data segment.
The association of virtual memory regions with segment regions is called *mapping*.

For performance reason, RVM does not write *directly* the committed image of virtual
memory regions back to the data segment. Doing so would generate excessive 'seek' operations
on the magnetic head of the disk, since the access pattern can be random. Disk head seeks
are expensive operation and should be minimized. In the case of RVM, this is achieved by
writing *new value* of memory ranges as *records* in an *append-only*, *sequential* log.
This is called *flush* operation. Log is also on stable storage (disk or battery-backed
ramdisk). Once the new values are written on log (flushed), they become persistent across
program crashes. From time to time, records on the log are processed and applied onto the
data segment. After that, the log space used by the records can be re-claimed for further
use. This is called *log truncation*. Log truncation is necessary, without it the log will
grow unboundedly.

## RVM application programming interface

To facilitate our discussion, let us first review briefly the RVM application programming
interface. Details of the interfaces can be found in [RVM Library](rvm_library.md) and
[RVM Functions](rvm_functions.md).

```c
rvm_options_t   options;
char            *version;
rvm_region_t    region;
rvm_tid_t       *tid;
char            *dest, *src;
unsigned long   length;

rvm_initialize(version, options)

rvm_map(region, options);

rvm_begin_transaction(tid, restore);
rvm_begin_transaction(tid, no_restore);

rvm_set_range(tid, dest, length);
rvm_modify_bytes(tid, dest, src, length);

rvm_end_transaction(tid, flush);
rvm_end_transaction(tid, no_flush);
rvm_abort_transaction(tid);

rvm_flush();
rvm_truncate();

rvm_unmap(region);

rvm_terminate();
```

Application declares regions of VM that it wants to have a persistent image by calling
`rvm_map()`. There are two possible modes for `rvm_map()`: `copy` and `no_copy`.
The former is the default and will cause the corresponding region in the segment device
to be read into virtual memory. The latter is used when the application is sure the image
in the segment device is useless (e.g., the application is going to replace it immediately).

Application declares the beginning of a transaction by calling `rvm_begin_transaction()`.
The transaction can be committed by calling `rvm_end_transaction()`, or be aborted by calling
`rvm_abort_transaction()`. In addition, the application must inform RVM the memory location
that will be affected by the transactions. These can be done by calling `rvm_set_range()`
or `rvm_modify_bytes()`. RVM guarantees that if the transaction commits, then all the
modifications made on declared ranges will be made effective atomically; conversely,
if the transaction aborts, then none of modifications will be made effective.

Normally, when transactions are aborted, the *old value* in the affected memory ranges
will be *restored*. So, a default option for `rvm_begin_transaction` is `restore`.
However, in some case, the application can be sure that it will never abort the transaction.
For example, if there are no concurrent transaction, the application can be sure it will
never abort the transaction explicitly. For these cases, the application can start the
transaction with the `no_restore` mode.

If the application wants the committed transaction to also be recorded in the log device
(stable storage) *immediately*, it should commit the transaction with `flush` mode.
In this mode, the `end_transaction()` calls return only after the transaction records
are written to disk. However, there are cases when the application can accept slight
chance of loss of permanency. In those case, it can end the transaction with a
`no_flush` mode. There will be a big payoff in performance, as the function
`rvm_end_transaction()` will return immediately, without having to wait for the slow
disk I/O to complete. RVM will put the committed transaction in a queue and write them
out to the log device later. This is called *lazy commit* and it provides a *bound
persistence*. The committed transactions may be lost if the program crashed before the
queued transaction gets a chance to be written out to log. Note that although the
transaction is lost, it is lost atomically. Upon the next restart of the application,
RVM will restore an old consistent view of the virtual memory.

Commit transaction lazily also gives more opportunity to inter-transaction optimization.

To force all previously committed transaction records to be written out to log device,
the application can call `rvm_flush()`. Also, whenever the application commits a new
transaction with `flush` mode, there will be a side effect that all previous transactions
on the queue will also be flushed.

The log device is only a temporary stable storage for committed transactions:
its primary purpose is to allow records be written on disk sequentially.
Eventually, these records need to be applied to the data segment. The process of applying
transaction log records to data segment is called *log truncation*. RVM will carry out
log truncation automatically when the log is too full. Users can specify the option
`truncate` to set the threshold. Alternatively, the application can initiate a truncation
by calling `rvm_truncate()`. Also, note that RVM will do a log truncation upon start up
and `rvm_map()` operations. This is to ensure the data segment has the latest updates
before it gets mapped into virtual memory.

## Structure of Log

### Log pointers and limits

The log device can be:
- regular Unix file
- raw partition
- raw partition in a dedicated disk

It is recommended to use the third option, raw partition in a dedicated disk.
As we see in the discussion, the purpose of the log device is to make the disk I/Os
as sequential as possible; having other simultaneous disk I/O will defeat this purpose.

Meta data of the log are collectively recorded on the *log status block* which is
located at the beginning of the log. It is offset by either `FILE_STATUS_OFFSET` or
`RAW_STATUS_OFFSET`, depending on the types of the log devices. The two constants are
currently 0 and 8192 bytes (16 sectors) respectively.

Log records are appended sequentially on log area after the log status block. The area
starts from `log_start` and is of size `log_size`.

XXX log_struct figure
*Structure of the log*

As new records are appended, the log pointer `log_tail` will be advanced. `log_head`
points to the first record in the log that is not yet subjected to truncation
(*current epoch*). If truncation is not in progress, both of the pointers
`prev_log_head` and `prev_log_tail` will have a zero value.

Currently, the log are truncated in a way called *epoch truncation*. In one atomic
operation, the function `new_epoch()` will bracket all the current log records by
`prev_log_head` and `prev_log_tail`, those records are said to be in *truncation epoch*
and are subject to truncation. At the same time `log_head` will be advanced to
`log_tail`, effectively resetting the *current epoch* to be empty. New log records may
be appended by concurrent threads while truncation is in progress, and `log_tail` will
advance to new position.

XXX log_trunc figure
*Truncation of the log*

It is important to use the log pointer to delimit the log into truncation epoch and
current epoch as we don't want the log truncation process to interfere with the log
flushing process.

If the truncation goes well and all the records are successfully applied to the data
segment. The log pointers `prev_log_head` and `prev_log_tail` will be reset to zero.
The area that was once occupied by those truncated records are now reclaimed.

However, it is possible that the program crashes while truncation is in progress.
RVM will know this upon next restart, by finding the two log pointers
`prev_log_head` and `prev_log_tail` are non-zero. It will then redo the truncation.
Truncations can always be redone as it is just writing the same value on the same
location again.

### Log records

There are five different types of log records. Each of them has a corresponding in-core
structure defined. They are listed below:

| Generic record | `rec_hdr_t` | 20 bytes |
| Transaction header | `trans_hdr_t` | 48 bytes |
| New value record | `nv_range_t` | 56 + length + pad bytes |
| Record end marker | `rec_end_t` | 28 bytes |
| Wrap marker | `log_wrap_t` | 24 bytes |
| Special log record | `log_special_t` | 24 + special + pad bytes |

*Six record types and their in-core structure*

Generic record (`rec_hdr_t`) does not actually exist in the log. It contains the common
first four fields as the other five 'real' records. When RVM scans the log and finds a
record, it may not know the type of the record. It can always cast the record as a generic
record, and then interpret the first four fields to determine the appropriate action.
For example, it can know the types of the records by looking at the first field.
(The common four fields are `struct_id`, `rec_length`, `timestamp` and `rec_num`).

In the five 'real' records, type specific fields will follow the common four fields.

```c
typedef struct
    {
    struct_id_t     struct_id;          /* type of entry */
    rvm_length_t    rec_length;         /* record length */
    struct timeval  timestamp;          /* timestamp of record entry */
    long            rec_num;            /* record number of entry */
    }
rec_hdr_t;

typedef struct
    {
    struct_id_t     struct_id;          /* self-identifier */
    rvm_length_t    rec_length;         /* log record length, displacement to
                                           end mark */
    struct timeval  timestamp;          /* timestamp of log entry */
    rvm_length_t    rec_num;            /* record number of log entry */
    rvm_length_t    num_ranges;         /* number of ranges in record */
    struct timeval  uname;              /* uname of transaction */
    struct timeval  commit_stamp;       /* timestamp of commit */
    rvm_length_t    n_coalesced;        /* count of coalesced transactions */
    rvm_length_t    flags;              /* mode and optimization flags */
    }
trans_hdr_t;
```

`trans_hdr_t`, `nv_range_t` and `rec_end_t` represent the *transaction header*,
*new value range header* and *record end marker*. They are the majority of records
on the log. Each transaction will be written in a sequence of records: it begins with a
transaction header, follows by one or more sub-record (the new value range record) and ends
with a record end marker. This is for the simple case when the transactions are not split
by a wrap marker in between. If the transaction is split by a wrap marker, it will have two
sequences of records, each begins and ends with a transaction header and end marker.

XXX log_records figure
*Transaction records*

New value records have variable lengths, each of them begins with a new value range header
and follows by the actual data. The actual data is the image of the virtual memory range
that the transaction has committed. The header is of type `nv_range_t` and is 56 bytes long,
the actual data length varies. Note that RVM may pad some bytes after the actual data so
that the next sub-record will be word-aligned.

To allow RVM to scan the log in both forward and backward direction, different forward and
backward links are provided. Generally speaking, `rec_length` is a forward link.
For transaction header, it is the forward link to the end marker; for new value record,
it is the forward link to the next new value record (or record end marker). However,
for end record marker, this value means the *backward* link to the transaction header.
(RVM is somewhat inconsistent in naming here). On the other hand, `sub_rec_len` is a
*backward* link to the previous sub-record. In new value range header, there is the third
length field: `length`, it is the length of the actual data. `length` may not be equal to
`rec_length` because of the padding.

XXX record links figure
*Forward and backward links of records*

The fourth possible record type that could be found on log is the wrap marker.
Note that wrapping may not happen exactly at the end of the log, it may happen a little
bit earlier. In the latter case, the bytes follow the wrap marker will all be written
with value `0xff`.

The fifth possible record type is special log record. Currently, the only special log
record is segment mapping descriptor (of type `log_seg_t`). RVM allows more than one data
segment for one log. To support this, every new value range will be tagged with a
`seg_code` which indicates to which segment that range should be applied to.
`seg_code` is only a shorthand of the segment. Details of the segment (`num_bytes` and
`name`) will be recorded in the segment mapping descriptor. Segment mapping descriptors are
inserted into the log whenever a new data segment is mapped the first time. At the time of
log truncation, RVM will use those segment mapping descriptors to reconstruct the necessary
information to open the segment device.

### Log wrapping

When `log_tail` approaches the end of the log, RVM will choose an appropriate point to do
a *wrapping*. This is done by writing out a wrap marker. After that, RVM will starting
appending new records from `log_start`. There will be no risk of overwriting onto useful
records because the old records should have been truncated long before. Even if they are not
truncated yet, RVM will call an internal function `wait_for_space()` to force a truncation.

Log wrapping can happen *between* transaction. In this case, it simply leave a wrap marker
(of type `log_wrap_t`) and start a brand new transaction at the beginning of log
`log_start`.

XXX wrapping figure
*Three different possible wrapping*

It can also happen *within* a transaction. There are two possibilities: between different
new value records or *splitting* a new value records. Since a range can be very long,
splitting new value records prevents leaving too much space not used by inserting the wrap
marker too early.

In the first case, after the current new value records are written, a record-end marker is
inserted into the log, and then followed by a log-wrap marker. Another transaction header
will be inserted at `log_start` before the remaining new value records are written.
Effectively, the same transaction will have two transaction headers: the first is written
before log wrapping, the second is written after log wrapping. The `flag` field of the
transaction headers helps to indicate this situation. The first transaction header will
only have `FIRST_ENTRY_FLAG` set, while the second transaction header will only have the
`LAST_ENTRY_FLAG` set.

For the latter case where a new value range is split, each of the two portions of the data
will has its own new value range header. The first portion of the splitted range is written
followed by a record-end marker and a wrap marker. The second transaction headers are then
written at `log_start`, followed by the second portion of the splitted range. The remaining
new value records then follows. There is a flag `is_split` in the new value record header
indicates that this happens.

## Internal Data Structure

Before we describe the important data structures used by RVM, we will
first discuss some of the building blocks of these data structures.  In
the next section we will discuss the generic list and tree data structures.
In the following sections we will discuss the synchronization primitives used
by RVM that enable multi-threading for RVM.

### Generic data structure

A lot of the RVM data structure are organized as lists or trees. To support that,
RVM internally defined a generic list type as well as a generic tree type.

Generic list is declared as `list_entry_t`, while generic tree node is declared as
`tree_node_t`. Supporting routine for them are `move_list_entry()` and `tree_insert()`
etc.

Each of these generic list/tree will contain enough topological information for it to be
manipulated as a list-node or tree node. Generic list and tree are typically not allocated
as themselves, rather, they are the first member of other specific data structures that
need to be organized. For example, the region descriptor `region_t` is declared with
`list_entry_t` as the first member of the structure. That way, the region descriptor can
be manipulated as a list element.

```c
seg_t     seg;
region_t  region;

move_list_entry(NULL, &seg->map_list, &region->links);
```

To move a region descriptor `region` to the mapped region list (`map_list`) in the segment
descriptor `seg`.

### Support for multi-threading

RVM is designed to be thread-safe: application is responsible for protecting access to its
data structures by concurrent threads, but can otherwise use RVM function freely.
RVM will synchronize its internal operations. This is done by using the following three
primitives:

#### Mutex

Internal data which may be accessed concurrently are protected by mutex of type `RVM_MUTEX`.
Routines accessing those data are required to do a `mutex_lock()` before, and do a
`mutex_unlock()` after. Alternatively, they can use the macro `CRITICAL()` to bracket
their access to critical data.

```c
RVM_MUTEX    lck;

mutex_init(&lck);   /* initialization */
...

mutex_lock(&lck);   /* begin    lck crit. sec */
  /* body */;           /* codes in lck crit. sec */
mutex_unlock(&lck); /* end      lck crit. sec */

                    /* alternatively use macro */
CRITICAL(lck,       /* begin    lck crit. sec */
  /* body */;       /* codes in lck crit. sec */
);                  /* end      lck crit. sec */
```

#### Read/Write Lock

Sometimes a data can be read by many threads simultaneously, provided none of them attempts
to update on the data. This kind of concurrency is supported by read/write lock of type
`rw_lock_t`. Readers access those data with `rw_lock(...,r)`, they will be allowed to
proceed immediately if there are no other writer, otherwise, they will be blocked until
the writer releases the lock. Writers access those data with `rw_lock(...,w)`, they will
be allowed to proceed immediately if there are no other readers or writers, otherwise,
they will be blocked until all of them release the lock. Both readers and writers have
to do a `rw_un_lock()` when they are done accessing the protected data.

```c
rw_lock_t     rwl;

init_rw_lock(&rwl); /* initialization */
...

/* reader */
rw_lock(&rwl,r);    /* begin rwl crit. sec. */
  /* body */;           /* read immediately if no other */
  /* body */;           /* writer, otherwise, block until */
  /* body */;           /* it releases lock */
rw_unlock(&rwl,r);  /* end   rwl crit. sec. */

...
/* writer */
rw_lock(&rwl,w);    /* begin rwl crit. sec. */
  /* body */;           /* write immediately if no other */
  /* body */;           /* writer/readers otherwise block */
  /* body */;           /* until they release lock */
rw_unlock(&rwl,w);  /* end   rwl crit. sec. */

...
RW_CRITICAL(rwl, mode, /* begin rwl crit. sec., mode=r|w */
  /* body */;
);                     /* end   rwl crit. sec. */
```

#### Condition variable

Two concurrent threads can synchronize their actions by using the condition variable of
type `RVM_CONDITION`. Thread A can sleep by calling `condition_wait()`, waiting to be
waked up by thread B when it calls `condition_signal()`.

This technique is used for the asynchronous log truncation. A separate thread will execute
the code of `log_daemon()` if there are multi-thread support (either mach's cthread,
or unix's lwp or pthread). Normally this thread is block waiting, other thread(s) will run.
From time to time, when transactions are committed, the other thread(s) will check and
see if the log is too full, if it is, it will wake up the `log_daemon()` which will then
truncate the log asynchronously.

```c
RVM_CONDITION     code;

/* thread of log_daemon() */
  CRITICAL(lock,
    {
    while (state == rvm_idle)
      condition_wait(&code, &lock);
    });
  /* async. log truncation, or terminate
   * log daemon here */

/* thread of log_tid() */
  CRITICAL(lock,
    {
      if (need_truncation) {
        if (state == rvm_idle) {
          state = truncating;
          condition_signal(&code);
        }
      }
    });
```

### Important Data Structures

The following structures are all declared in the file `rvm_private.h`. The important
data structures are the following.

#### Segment Descriptor (seg_t)

There can be more than one segment per process. Each of the segment has a segment
descriptor. These segment descriptor are linked together as a list, and the list header
is the global variable `seg_root`.

The segment descriptor contains the header `map_list` for an important list: list of
mapped region, which be discussed in more detail [here](#region-descriptors-region_t).

There can be more than one segment per log. In fact, each individual new value record
can be applied to different segment. This is achieved by tagging a short-name of segment
`seg_code` with each new value record. The short name of each segment is also stored
in its own descriptor.

#### Log descriptor (log_t)

Like the segment descriptors, log descriptors are also connected as a list (header is a
global variable `log_root`). However, currently RVM only support one log in a process,
which is pointed to by the global pointer `default_log`.

`log_status` is the in-core copy of the log status block. Similarly, `trans_hdr`,
`rec_end`, `log_wrap` are the area for preparing the corresponding records on log.

`log_buf` is the log recovery buffer descriptor.

Log descriptor contains headers for three important list: `tid_list`, `flush_list`
and `special_list`. The first links up transaction that have begun but not yet ended.
The second links up transaction that have been committed but not yet flushed.

#### Log status area (log_status)

Log status has a in-core copy as well as in-disk copy. The in-core copy is declared
as type `log_status_t`. The in-core copy is stored as a sub-structure of the log
descriptor, and the in-disk copy is written on the log status block. Ideally, we would
want the in-core copy to written to disk every time the log status change, however,
doing so will require a lot of disk head seeks between the log status block and the
log tail, which defeat the purpose of the log, so RVM updates the on-disk log status
block only every UPDATE_STATUS (currently the value is 100) of status change. This is
keep tracked by the count `update_cnt`. Because of that, RVM will not complete trust
on the value in the log status block.

Log status area records important log pointers: `log_head`, `log_tail`,
`prev_log_head`, `prev_log_tail` and log limit: `log_start` and `log_size`.
Besides, it also records a huge numbers of consistency check fields, transaction
statistics and log statistics.

#### Area for preparing log records

Each of the log record types has a corresponding in-core data structure.
They are used to prepare the records before they are written out to log.
There is only one copy for `trans_hdr_t`, `rec_end_t` and `log_wrap_t` per-log and they
are part of the log descriptor. For new value records, areas are required for the headers
and the actual data and they are per-range. Headers are of type `nv_range_t` and are
stored in the range descriptors. Actual data are stored in a separately allocated buffer.
Log special records are associated with the log, and they dynamically allocated and are
organized as a list (header: `log_special_list`) in the log descriptor.

#### Log recovery buffer

Log recovery buffer (also called log buffer) is used in log truncation.
It is used to keep an image of log records for processing. The descriptor of the buffer
is called log recovery buffer descriptor (type `log_buf_t`). It is a sub-structure of log
descriptor. The actual buffer is pointed to by the field `buf`, the length of the buffer
is stored by the field `buf_len`.

Two routines can be used to fill and refill the log recovery buffer:
- `init_buffer(log, offset, direction, ...)`
- `refill_buffer(log, direction, ...)`

The log can be scanned in two directions. This is indicated by the argument `direction`
of the two routines.

Upon returns of these two routines, `log_buf->ptr` will be the index into log buffer
that corresponds the current log position, and `log_buf->offset` will correspond to the
log offset of byte 0 in log buffer. `log_buf->offset` will be sector aligned.

XXX log_buf figure
*log buffer and init_buffer()*

The following is a typical sequence code in log truncation:

```c
...
r_length      = init_buffer(log, off, REVERSE, ...);
log_buf->ptr -= sizeof(rec_end_t);
rec_end       = (rec_end_t *) &log_buf->buf[log_buf->ptr];
if (rec_end->struct_id == rec_end_id) {
  ...
```

In the code, `off` is the current log position.  Routine
`init_buffer()` is instructed to initialize the buffer by scanning
the log in the reverse direction.  When it returns, `r_length`
number of bytes are read into the log recovery buffer (pointed to by
`log_buf->buf`), `log_buf->ptr` is index of the byte in log
buffer that corresponds to `off`.  Assume the routine here is
expecting a end record in log, it decrements `log_buf->ptr` by the
known size of `rec_end_t` so as to point to the end record.  After
the decrement, the address of `log_buf->buf[log_buf->ptr]` should
be the beginning of the in core image of the end record.  It type-cast
it as `(rec_end_t *)` and stores it in `rec_end`.  After that it
can manipulate the record as it likes.  In the example, it examines
and see if `struct_id` of the end record is really a `rec_end_id` ...

#### Region descriptors (region_t)

Each segment will have a list of regions that are mapped. So in each segment descriptor
(type `seg_t`) there is a member `map_list` which is the head of this list.
Each node in the list is a region descriptor of type `region_t`.

It records the `offset` and `end_offset` of the region in the data segment, as well as
the `vmaddr` and `length` of region that is mapped to.

The member `n_uncommit` records the number of uncommitted range in this region,
it is incremented every time application does a `rvm_set_range()`, it will be decremented
when a transaction is committed or aborted. When application tries to do `rvm_unmap()`,
this value must be zero otherwise it will return RVM_EUNCOMMIT.

`region_t` may be confused with `rvm_region_t` and `mem_region_t`. We will explain the
former in the following and the latter in the next section. `rvm_region_t` is part of the
API, it is the structure used by the application to indicate the mapping parameters:
`data_dev`, `offset`, `length`, `vmaddr` and `no_copy` etc.

#### Memory region descriptors (mem_region_t)

In addition to the per segment region list described above, there is also a per process
memory region tree. It is used to record the regions in the virtual memory space that are
mapped, and it is arranged in the order of vm address.

When application does a `rvm_map()`, this tree is consulted to make sure the requested
virtual memory region is not already mapped. If it is already mapped,
RVM_EVM_OVERLAP will be returned. Also, when application does a `rvm_set_range()`,
this tree is consulted to make sure the requested range is totally contained within a
single virtual memory region. If it isn't, RVM_ENOT_MAPPED will be returned.

The root of the tree is `region_tree`. Each node of the tree is of type `mem_region_t`.

#### Transaction descriptor (int_tid_t)

Whenever the application does a `rvm_begin_transaction()`, rvm will allocate a
transaction descriptor of type `int_tid_t`. These descriptors are linked up into list.
Before the transaction commits, the descriptor is in the list headed by `log->tid_list`.
If the transaction is committed, it will be moved to another list headed by `log->flush_list`.
If the transaction record is flushed, it will be removed from the flush list.

Typically, the application may inform RVM one or more memory range which will be affected
by the transaction. This is done by issuing one or more `rvm_set_range()` or
`rvm_modify_bytes()` after `rvm_begin_transaction()`. Internally, these memory ranges
are organized as a tree rooted at `range_tree` (of type `tree_root_t`). The tree root is a
field of the transaction descriptor.

`int_tid_t` may be confused with `rvm_tid_t`. The latter is the transaction identifier
used by applications while the former is used internally by RVM. Although `rvm_tid_t`
contains a pointer to `int_tid_t`, applications are not expected to use or touch the
pointer.

#### Modification range descriptor (range_t)

When application specifies modification range by `rvm_set_range()` or `rvm_modify_bytes()`,
the vm address (`range->nv.vmaddr`) and the length (`data_len`) of the range is stored
internally in the range descriptor (`range_t`). When the transaction commits, the content
of virtual memory pointed to by (`range->nv.vmaddr`) will be copied to the buffer pointed
to by `range->data`. The same buffer has another purpose: if the transaction is in
`restore` mode, the buffer is used also for saving the old image of VM. In case the
transaction is aborted, the old image can be restored.

Range descriptors are connected together as a tree, with the tree root in the corresponding
transaction descriptor. If the optimization flag `RVM_COALESCE_RANGES` is specified, the
tree is sorted according to the vm address, and ranges are coalesced before they are
inserted into the tree. If `RVM_COALESCE_RANGES` is not specified, the tree is simply
sorted by the range number.

There is also an area of type `nv_range_t` called `nv` which is used as the in-memory
image of the nv range record on log.

#### Storage device region node (dev_region_t)

This is not to be confused with `region_t` and `mem_region_t`. `dev_region_t` is used only
in log truncation. During log truncation, all sub-records of all transactions in the
truncation epoch are processed. A tree called `mod_tree` is built. Each node of the tree is
of type `dev_region_t`. The tree is sorted according to the segment offset. The purpose
of the tree is to make the disk write operation on the segment device sequential.

#### Device descriptor (device_t)

Device descriptor (of type `device_t`) contains the details of the physical devices: name,
handle, size. Also, it contains the pointers to the gather-write I/O vectors.

## RVM in action

### rvm_initialize()

Global lists and trees are initialized (`seg_root`, `log_root`, `region_tree`).
A log truncation (`log_recover()`) is performed.

### rvm_map()

There is always a log truncation at the begin of mapping. This is necessary to ensure
the mapping will get the latest committed image from the data segment.
It also checks if application has specified a new segment device, if so, it will create
a special record of type `log_seg_t` and queue it on `log->special_list`. This record
will be flushed to the log before any next transaction.

Application may or may not specified the mapped-to address for the region.
If it is not specified, `rvm_map()` will do a `page_alloc()` to find a VM address for
the request region. Either case, the mapped-to address will be stored in `region->vmaddr`
as well as `mem_region->vmaddr`.

Before carrying out the mapping, `rvm_map()` also need to check that the requested
mapping is not overlapped with existing mapping in both the VM address space as well as
the segment offset space. The former is detected with the aid of `region_tree` while the
latter is detected with the aid of `seg->map_list`. If there are overlapping,
RVM_EVM_OVERLAP will be returned in the former case and RVM_EOVERLAP will be returned
in the latter case.

Finally, the content of the data segment at the specified offset is copied into the VM
space (unless `no_copy` mode is specified).

### rvm_begin_transaction()

A transaction descriptor (of type `int_tid_t`) will be allocated and inserted in
`log->tid_list`. The address of that descriptor is also noted in `rvm_tid->tid`.

### rvm_set_range() and rvm_modify_bytes()

The range declared must be totally included inside a mapped region, otherwise
RVM_ENOT_MAPPED will be returned. This is done by consulting `region_tree`.

A new range descriptor (of type `range_t`) will be allocated and inserted into
`tid->range_tree`. Depending on the optimization flag of the transaction,
the range tree may be sorted by vm address or sorted by range numbers.

If transaction is in `restore` mode, the current image of VM in the range will be first
saved to a buffer pointed to by `range->data`.

VM address of the range declared is stored in `range->nv.vmaddr`.

### rvm_end_transaction()

When application calls `rvm_end_transaction()`, for each of the ranges in
`tid_range_tree`, the vm area pointed to by `range->nv.vmaddr` is copied to
`range->data`. This is to ensure the atomicity of transaction: when
`rvm_end_transaction()` returns, all declared ranges have been copied to a separate
location. If the program crashes before `rvm_end_transaction()` returns, then the data
segment will contain the old value of the declared ranged. Applications are free to write
new data on the ranges after `rvm_end_transaction()` returns.

The transaction descriptor `tid` is dequeued from `log->tid_list` and is queued to
`log->flush_list`.

If RVM_COALESCE_TRANS flag is set for the transaction, an inter-transaction coalescing
will be carried out.

If the transaction is committed in `flush` mode, `flush_log()` is called.

### flush_log()

Every transaction descriptors in `log->flush_list` are written out to the log,
this is done by the routine `log_tid()`. First, the transaction header of type
`trans_hdr` is built and added to the gather write vector `dev->iov`. Then, for every
ranges in `tid->range_tree`, the new value range header of type `nv_range_t` is built
and added to the gather write vector, followed by the actual new value data. After all
the new value records are written, a record end marker (of type `rec_end_t`) is built
and added to the gather write vector.

If log wrapping is needed, there will be intervening record end marker,
log wrap marker and transaction header record inserted.

### rvm_truncate()

Log truncation is carried out by `log_recover()`. For each segment, a modification tree
`mod_tree` is built. Each node is of type `dev_region_t`, it is arranged in order of
segment offset. For each new value records of each transactions on the log, a node of
type `dev_region_t` is built and inserted into the modification tree. If there are
overlaps in segment offset, the overlapped portion is not repeatedly inserted into the
modification tree.

When all the log records bracket by the log pointers `prev_log_head` and
`prev_log_tail` are scanned and inserted into the modification tree.
The whole tree is written to data device with `apply_mods()`.

## RVMUTL

The program `rvmutl` is a utility program that allows:

- initialization of a log
- debug via the log

The following is an incomplete list of commands available in `rvmutl`:

- `init_log`: initialize a log
- `open_log`: open an existing log
- `status`: display information on log status block
- `head`: locate and display the first record of current epoch
- `tail`: locate and display the last record of current epoch
- `prev (n)`: locate and display previous (n) record(s)
- `next (n)`: locate and display next (n) records(s)

In the rvm source codes, you can often see the global variable `rvm_utlsw`.
RVM is designed in mind that there are two possible types of users for rvm's routines:
ordinary applications or the program `rvmutl`. This variable allows a differential
treatment on exception condition. For example, in the routine `scan_forward()`,
if the record type is found not to be one that the routine is expecting (possibly
means a corrupted log) the program will:
- abort, if it is called by an general application
- continue with error return, if it is called by `rvmutl`

The latter treatment allows manual handling of log corruption via `rvmutl`.
