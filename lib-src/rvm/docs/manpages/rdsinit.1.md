% RDSINIT(1)

NAME
====

rdsinit - RDS heap initialization utility

SYNOPSIS
========

**rdsinit** *log* *data\_seg*

**rdsinit** *log* *data\_seg* *datalen* *saddr* *hlen* *slen* *nl* *chunk*

**rdsinit** *-f* *log* *data\_seg* *datalen* *saddr* *hlen* *slen* *nl* *chunk*

DESCRIPTION
===========

`rdsinit` is a utility that constructs an initialized RDS heap in an RVM
segment. It is intended to create a structure that can be loaded by
`rds_load_heap`.

There are three different ways of using rdsinit.
General users are expected to use first two interactive modes, where
users may supply parameters for the rds heap interactively or on command
line arguments. However, in both cases, users will be asked
interactively to confirm their choice of parameters before rdsinit goes
ahead to make any permanent change. These are the preferred modes of
using rdsinit. Script writers, however, may prefer to supply all the
parameters on the command line and *no* confirmation required for those
parameters. This is accommodated in the third mode where an additional
switch of -f (firm) is supplied on the command line.

In any case, two
command-line parameters are always required: `log` and `data_seg`. The
former is the name of the RVM log, which must have previously been
initialized by `rvmutl`; the latter is the name of the data segment that
will be initialized with an RDS heap. If either is missing, a command
line error is printed. If the log has not been initialized, an RVM error
will result. A short message indicating RVM initialization succeeded is
then printed. Both the log and data segment can be regular files or raw
partitions.

After the name of log and data segment, there are six other
numeric parameters required. They are summarized here and will be
explained one by one in the following paragraphs:

datalen
:   Length of data segment

saddr
:   Starting address of rvm

hlen
:   Heap region length

slen
:   Static region length

nl
:   Number of lists of free block

chunk
:   Chunk size

While entering these six numeric parameters, either on command line on
via the interactive prompt, users may use numeric number in hexadecimal,
decimal or even octal notation. Hexadecimal numbers are preceded by
`Ox`, decimal numbers are preceded by nothing and octal numbers are
preceded by `0`.

Special note for long time rdsinit user: the old
rdsinit automatically assumed `saddr`, `hlen` and `slen` parameters
supplied on command lines are in hexadecimal and *did not* require the
prefix `0x`. *This is no longer true with this version of rdsinit.*

Users specify the length of the data segment with the parameter
*datalen*. Again, old version of rdsinit did not require this parameter
if the data segment was a regular file and it existed already at the
time of running rdsinit. This special case is eliminated: *length of
data segment must to be supplied in all circumstances*.

Starting address
of rvm, or *saddr*, is where heap and static region will be mapped into
virtual memory. Heap region is located right at the starting address,
while static region is located at starting address plus heap region
length. Users may need to have some knowledges of the overall layout of
the virtual memory use by the system before they can make the right
choice of starting address. For example, the starting address of rvm
must be much larger than the largest possible break point of your
application, and it should not be in conflict other uses of virtual
memory (such as use by shared libraries). It must also be on a page
boundary. In CMU, we use 0x20000000 (536870912) with Linux and BSD44, or
0x70000000 (1879048192) with Mach. It is possible to choose other
values, but you have to choose them carefully.

Length of regions of heap
and static are specified by the parameter *hlen* and *slen*
respectively. They both must be integral multiple of pagesize of the
system. Also, the combined length of the two regions must be smaller
than the length of data segment minus one extra page.

Note that the
above three parameters: saddr, hlen, slen, are permanent. They can\'t be
changed without re-initizing (and brain-wiping) the data segment.

The next two parameters: *nl* and *chunk* are related to underlying
structure of management of the heap. RDS uses the Quick Fit method for
heap allocation. In this method, free blocks are maintained by a number
of free lists, each list for one particular size of free blocks.
Specifically, there will be `nl` free lists, and each of them will have
free blocks of size `1..nl` chunk respectively.

Chunk size must be
integral multiple of `sizeof(char *)`, and be at least
`RDS_MIN_CHUNK_SIZE`. Number of lists must be at least
`RDS_MIN_FREE_LISTS`. For example, a reasonable choice is to have 100
free list with chunk size 32 bytes.

Once all the parameters are chosen,
rdsinit will ask user for confirmation before it goes ahead and make
permanent change on the log and data segment. Note in the following
example that those numerical arguments are presented in both hexadecimal
and decimal (in bracket). It is safe to quit at this point and no
permanent changes will be made.

The following parameters are chosen:

       length of data segment:    0xf5000 (   1003520)
      starting address of rvm: 0x20000000 ( 536870912)
                     heap len:    0xf0000 (    983040)
                   static len:     0x4000 (     16384)
                       nlists:       0x64 (       100)
                   chunk size:       0x20 (        32)
    Do you agree with these parameters ? (y|n|q) y

If user supplied the -f (firm) switch on command line, this last
confirmation will not show up.

SEE ALSO
========

`rds_init_heap (3)`, `rds_load_heap (3)`, `rds_zap_heap (3)`,
`rvm_create_segment (3)`, `rvm_load_segment (3)`, `rvmutl (1)`

AUTHOR
======

David C. Steere, created man page

Yui W. Lee, modified (Sept 1997)
