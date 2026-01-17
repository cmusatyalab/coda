% RVMUTL(1)

NAME
====

rvmutl - maintenance utility for RVM log

SYNOPSIS
========

**rvmutl** *log*

**rvmutl**

DESCRIPTION
===========

The program **rvmutl** is the maintenance utility for RVM log. Its
priniciple features are log creation and maintenance, but there are also
status and statistic display commands, as well as printing commands for
log records. There is also a search facility for modifications to
segments. You may find **rvmutl** a great debugging tools.

There are two ways
to use **rvmutl**: You may supply the name of the RVM log on the command
line, or you do not supply the name on command line but specify the log
within **rvmutl** (see below).

**rvmutl** prompts for commands with a \*
(asterisk) prompt. Most commands have one or more parameters, which are
separated by one or more spaces. Some commands have abbreviations, these
will be specified in the command section.

For example, to examine the
status of a RVM log (called logfile in the example), you can do

    > rvmutl
    * open_log logfile
    * status

    Status of log:           /home/clement/linuxobj8/rvm-src/tests/logfile

      log created on:        Tue Nov  4 1997 17:31:10.244793
      log created with:      RVM Interface Version 1.3  7 Mar 1994
      (more display omitted ...)
    * quit

As another example, suppose you want to create a RVM log device on the
partition /dev/hdc7, of size 1 Megabyte, you can do

    > rvmutl
    * i /dev/hdc7 1M
    * quit

Note that in the preceding example, we use the abbreviation \"i\" for
the command \"init\_log\", and we can use the unit \"M\" to stand for
Megabyte.

I/O REDIRECTION
===============

You can redirect input and output as in typical Unix shells. For
example, the following command will have all display redirected to the
file allrec:

    * show all > allrec
    * 

COMMANDS
========

In the following, I will detail all the available commands:

all_spec
:   Show all special log records. Special log records are those
    non-transaction records, such as wrap-around marker and segment
    dictionary entry.

build_seg_dict [all] [clear]
:   <!-- -->

seg_dict  [all] [clear]
:   Build the segment dictionary by scanning through the log. Segment
    dictionary is a in-memory structure that maps segment code to segment
    file. Segment code is used in each transaction range as a short hand of
    the segment file involved. If the **all** option is specified, the whole
    log will be scanned, otherwise the scanning will stop when one segment
    dictionary entry is found. If the **clear** option is specified, the
    segment dictionary will be cleared before starting the scanning process.

close_log
:   Close the log file.

copy_log
:   This provide a handy way to copy an existing log to another file or
    partition. Since rvm logs start on different offset, depending on
    whether they are files or partitions, using stock Unix commands such as
    cp or dd sometimes is not as nice as using this command. Some meta data
    (such as name of log) in the log status area in the destination log will
    also be updated to reflect new meta data after log copying.

earliest
:   Show the earliest transaction record in the log.

init_log [*log* [*length*]]
:   Initialize a log if it does not exist, or re-initialize it if it does
    exist. The log will have the name *log* and the length of data area will
    be *length*. The actual size of the log file or partition will be
    slightly bigger because: 1. there is a log status area (of size 3
    sectors); and 2. for log used on raw partition, there will be an offset
    (of 16 sectors) to jump off possible disklabel of the partition.

    The *length* parameter is specified as an integer, optionally followed by a
    single character size code. The following size codes are defined: **K**
    or **k** for kilobytes, **M** or **m** for megabytes, **S** or **s** for
    sectors (512 bytes), **P** or **p** for pages (which have system
    dependent sizes) and **B** or **b** for bytes (default).

    If either parameter is not specified, **init\_log** will prompt for the
    missing information.

    RVM maintains the log continuously after creation. If you
    are going to re-initialize an existing log, make sure the log is empty
    before the re-initialization, otherwise, the transaction records in the
    log would be lost (i.e., they would not be applied to the data segment).
    You can apply all transaction records to the data segments and empty the
    log (also known as truncate in this manual) by the **recover** command.
    You can use the command **status** to check whether a log is empty.

find_earliest
:   Print the offset of the earliest transaction record in the log.

find_tail
:   Print the offset of the tail of the log.

head
:   Print the transaction record at the head of log. (Note that there could
    be even earlier than the head record \-- smaller record number and
    smaller time stamp. They are considered truncated from the log and can
    be overwritten upon them freely. They are still on the log just because
    it happens that rvm has not yet overwritten on them, and they may be
    useful for debugging. The command **earliest** prints the earliest
    record.)

log [*log*] [**no_tail**] [**no_update** | **update**]
:   <!-- -->

open_log [*log*] [**no_tail**] [**no_update** | **update**]
:   Open the rvm log named *log*. You must open a log before you can use
    most of the rest commands. The optional **no\_tail** switch turns of the
    search of tail of the log (for example, when the log tail is corrupted).
    The optional **no\_update** and **update** switch controls whether
    update will be made on the log and segment (they turn on the rvm
    internal global variable `rvm_no_update`).

n [*num*]
:   <!-- -->

next [*num*]
:   Print the next *num* records. If the optional argument *num* is not
    specified, then the next one record will be printed.

ns [*num*]
:   <!-- -->

next_sub_rec [*num*]
:   Print the next *num* sub-records. Sub-records are the individual ranges
    of a transaction. If the optional argument *num* is not specified, then
    the next one sub-record will be printed.

p
:   <!-- -->

peek [*file*] *offset* / [*csf*]
:   Peek on (or dump) the content of *file*. You must specify the
    argument *file* when you first peek or poke on a file, afterward you
    can omit the argument and rvmutl will continue using the current
    file. The argument *offset* determines from where the dump should
    start. The argument *csf* determines how the dump should be done, it
    comprises of three components: *c*, *s* and *f*; each of them
    represents the count, size, and format respectively. *c* is an
    integer and determines how many units, with size specified by *s*,
    should be dumped, and *f* specifies what is the dump format.\"

    In the following two examples, the first command dumps, from offset 0,
    eight bytes in hexadecimal format; the second command dumps two long
    integers in decimal format. Note that how the same file content can be
    dumped into different format.

        * peek 0 / 8xb
        00000000000: 0x18 0000 0000 0000 0x23 0x17 0x14 0x63
        * peek 0 / 2dl
        00000000000:           24   1662261027

    There are seven different possible sizes: **b** for byte, **l** for
    long, **s** for short, **O** for offset (two long integers representing
    the high and low order bits respectively), **f** for float, **D** for
    double and **t** for time value.
    
    There are nine different possible
    formats: **c** for character, **d** for decimal, **o** for octal, **x**
    for hexadecimal, **u** for unsigned, **f** for float, **D** for double,
    **O** for offset (two long integers representing the high and low order
    bits respectively) and **t** for time value.

    You may omit the arguments
    *c*, *s* and *f*, and rvmutl will use the defaults, which are 1, d
    (decimal) and l (long) respectively, for the three arguments
    respectively.

poke [*file*] *offset* / *cs* = *val* [*val* ...]
:   Poke on (modify) the content of *file*. You must specify the argument
    *file* when you first peek or poke on a file, afterward you can omit the
    argument and rvmutl will continue using the current file. The argument
    *offset* determines from where the modification should start. The
    argument *cs* determines how the modifications should be done, it
    comprises of two components: *c* and *s*; each of them represents the
    count and size respectively. *c* is an integer and determines how many
    units, as specified by *s*, should be modified.

    Examples:

        * poke 512 / 4db = 25
        * peek 512 / 8db
               512:  25  25  25  25   0   0   0   0

        * poke 512 / 2xl = 25
        * peek 512 / 8db
               512:  25   0   0   0  25   0   0   0

        * poke 512 / 2b = 0x26 0x27
        * peek 512 / 8db
               512:  38  39  38  39  25   0   0   0

        * poke 512 / b = "abcde"
        * peek 512 / 8db
               512:  97  98  99 100 101   0   0   0

    Note that in the second example, the character **x**, which indicates
    hexadecimal format in peek, is sliently ignored. To enter a hexadecimal
    number, you should us prefix \'0x\' as in the third example. Note also
    that how a pattern is repeated by using the argument *c* in first and
    third example.

    There are seven different possible sizes: **b** for byte,
    **l** for long, **s** for short, **O** for offset (two long integers
    representing the high and low order bits respectively), **f** for float,
    **D** for double and **t** for time value.

    You can also enter string by
    using the double-quote mark ("), as in the last example.

pr [*num*]
:   <!-- -->

prev [*num*]
:   Print the previous *num* records. If the optional argument*num* is not
    specified, then the previous one record will be printed.

ps [*num*]
:   <!-- -->

prev_sub_rec [*num*]
:   Print the previous *num* sub-records. Sub-records are the individual
    ranges of a transaction. If the optional argument *num* is not
    specified, then the previous one sub-record will be printed.

quite
:   Quit rvmutl.

read_status
:   Re-read the log status from on-disk log device (file or partition) to
    in-memory structure (`status`). Note that the command **status** show
    the content of the in-memory structure, but not the on-disk device. If
    you have made changes to the on-disk device, you may want to re-read
    them into the memory.

monitor [**clear**] [**no_update** | **update**]
:   You can specify memory ranges to be monitored by this command. When a
    memory range is monitored, there will be message printed out whenever
    there are modification within the range (E.g. in **recover**). The
    switch **clear** will clear all the monitor previous set. The switch
    **no\_update** and **update** carry the usual meaning (c.f.
    **open\_log**). You will be asked, interactively, for the addresses and
    formats for the ranges you want to monitor, you should use the syntax
    similar to **poke**. The following example shows how to use **monitor**,
    **show monitor**, as well as how **recover** prints out the
    modification.

        * open_log log3
        * monitor update
          More ranges (y or n [n])? y

          Enter list of addresses/format, terminate with null line
        :  0x200d7550/1l
        : 
        * show monitor

        Range    Address/format     Length
           1    0x200d7550/1dl          4

        * recover
        do_nv: data from log record 2101, range 52
          monitored range 1, 0x200d7550, length 4 matched by
          modified range 0x200d7550, length 4
          0x200d7550:    537286660

        change_tree_insert: inserting entire range
          monitored range 1, 0x200d7550, length 4 matched by
          modified range 0x200d7550, length 4
          0x200d7550:    537286660


        do_nv: data from log record 2101, range 51
          monitored range 1, 0x200d7550, length 4 matched by
          modified range 0x200ced40, length 8367
          0x200d7550:            0

        change_tree_insert: inserting entire range
          monitored range 1, 0x200d7550, length 4 matched by
          modified range 0x200ced40, length 8367
          0x200d7550:            0

        do_nv: data from log record 2101, range 50
          monitored range 1, 0x200d7550, length 4 matched by
          modified range 0x200cdcc4, length 16
          0x200d7550:            0

        change_tree_insert: inserting entire range
          monitored range 1, 0x200d7550, length 4 matched by
          modified range 0x200cdcc4, length 16

recover [**clear**] [**file**] [**no_update** | **update**]
:   Truncate the log. Truncation means applying the transaction records,
    that were logged on the log, to the data segment, and then reclaiming
    the space on the log used by those records.

set [**seg_dict**] *field* | *addr* = *val*
:   Set *field* (or memeory locateion at *addr*) to *val*. There are five
    fields that you can set: **head**, **tail**, **prev\_head**,
    **prev\_tail** and **log\_start**. (You can use **prev head** and **prev
    tail** to represent **prev\_head** and **prev\_tail** respectively.)
    Only the in-memory structure of the field is changed by this command,
    you can use **write\_status** to write the in-memory structure (status
    area) to disk.

s
:   <!-- -->

show
:   <!-- -->

    all
    :   all records

    all_records
    :   all records

    earliest
    :   earliest record

    head
    :   record at head of log

    mods *of* / *cs* [= *val*]
    :   <!-- -->

    modifications *of* / *cs* [= *val*]
    :   E.g.

        * show mods 0x2000005c / 8b

        Record number: 40756 modifies specified range:

          Modification range:        1   Log offset:    224664
            VM address:     0x2000005c   Length:        52
            Segment code:            1   Offset:      4188
            Record length:         108   Back link:     48

          0x2000005c:  74 -107   0   0   0   0   0   0

        * show mods 0x2000005c / 1b = 74

        Record number: 40756 assigns specified values

          Modification range:        1   Log offset:    224664
            VM address:     0x2000005c   Length:        52
            Segment code:            1   Offset:      4188
            Record length:         108   Back link:     48

          0x2000005c:  74

        * show mods 0x2000005c / 1b = 75
          -- no more records

    monitor
    :   all the range being monitored

    next
    :   next record

    next_sub_rec
    :   next subrecord

    ns
    :   next subrecord

    *num*
    :   by record number

    prev
    :   previous record

    previous
    :   previous record

    ps
    :   previous subrecord

    prev_sub_rec
    :   previous subrecord

    rec_number
    :   *num* by record number

    remaining
    :   all remaining records

    seg_dict
    :   segment dictionary

    seg_dictionary
    :   segment dictionary

    statistics
    :   statistics of rvm activities

    status
    :   status area of the log

    log_status
    :   status area of the log

    sr
    :   current subrecord

    sub_rec
    :   currect subrecord

    tail
    :   tail record

    timestamp
    :   by record timestamp (note: no effect)


sizeof *strcut* | **all**
:   Show the size of a the give structure *struct*, or size of all
    structure. The unit is byte. The following structure can be shown:
    **condition**, **device\_t** (**dev**), **dev\_region\_t**,
    **FLUSH\_BUF\_LEN**, **free\_page\_t**, **MAXPATHLEN**, **int**,
    **list\_entry\_t**, **log\_t** (**log**), **log\_buf\_t**,
    **LOG\_DEV\_STATUS\_SIZE**, **log\_dev\_status\_t**, **log\_seg\_t**,
    **LOG\_SPECIAL\_IOV\_MAX**, **LOG\_SPECIAL\_SIZE**, **log\_special\_t**,
    **log\_status\_t** (**status**), **log\_wrap\_t**, **long**,
    **MAX\_READ\_LEN**, **mem\_region\_t**, **MIN\_NV\_RANGE\_SIZE**,
    **MIN\_FLUSH\_BUF\_LEN**, **MIN\_RECOVERY\_BUF\_LEN**,
    **MIN\_TRANS\_SIZE**, **mutex**, **NUM\_CACHE\_TYPES**,
    **NV\_LOCAL\_MAX**, **nv\_range\_t** (**nv**), **NV\_RANGE\_OVERHEAD**,
    **page\_size**, **range\_t**, **rec\_end\_t**, **rec\_hdr\_t**,
    **RECOVERY\_BUF\_LEN**, **region\_t**, **rvm\_length\_t**,
    **rvm\_offset\_t**, **rvm\_options\_t**, **rvm\_region\_t**,
    **rvm\_tid\_t**, **rw\_lock\_t** (**rw\_lock**), **rw\_lock\_mode\_t**,
    **seg\_t**, **struct\_id\_t**, **int\_tid\_t**, **timeval**,
    **trans\_hdr\_t**, **TRANS\_SIZE**, **tree\_links\_t**,
    **tree\_node\_t**, **TRUNCATE**, **ulong**, **unsigned**.

status
:   <!-- -->

log_status
:   Show the log status area, which is the meta data about the log (include
    head offset, tail offset, space used by records, total log size, first
    record number, last record number, log creation time, last truncation
    time etc).

sr
:   <!-- -->

sub_rec
:   Show the current subrecord.

statistics
:   Show the statistics of rvm activities.

tail
:   Show the transaction record at the tail of log.

update
:   Turn on update (i.e. set to false the rvm-internal global variable
    `rvm_no_update`.) Update will not be made on the log and segment.

no_update
:   Turn off update (i.e. set to true the rvm-internal global variable
    `rvm_no_update`.) Update will be made on the log and segment.

write_status
:   Write out the in-memory log status structure to the log status block on
    disk.

BUGS
====

The command **find\_hole** is not yet documented.

Peeking using the format or size of time value (**t**) does not work.

The way to poke an offset value is not yet documented.

**sizeof** of some structs (e.g. **condition**, **int**) wrongly displays the
page size rather than the size of the structure.

The use of **no\_update** does not quite work. Rvmutl easily crashes when the
switch is used.

The exact semantic of **replay** command is not documented, and the command
itself crashes quite easily.

The use of **seg\_dict** in **set** is not yet documented.

The command **show timestamp** has no effect.

AUTHOR
======

Yui Wah LEE completely rewrote this man page (Nov. 1997)
