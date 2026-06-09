# RVM Installation Instructions

This appendix contains instructions to import and install RVM.  The
process is set up to be a compatible as possible with both Mach 2.6
and Unix BSD 4.3.  Support for other versions of Unix is limited, but
in general, RVM attempts to keep its system requirements to a minimum
to enhance portability.  Where necessary for specific versions of Unix
and related systems, special instructions are included below.

Please note that RVM is copyrighted by Carnegie Mellon University, and
the terms of distribution are specified by the following copyright notice:

``` text
       RVM: an Experimental Recoverable Virtual Memory Package
                 Release 1.3

      Copyright (c) 1990-1994 Carnegie Mellon University
             All Rights Reserved.

Permission  to use, copy, modify and distribute this software and
its documentation is hereby granted (including for commercial  or
for-profit use), provided that both the copyright notice and this
permission  notice  appear  in  all  copies  of   the   software,
derivative  works or modified versions, and any portions thereof,
and that both notices appear  in  supporting  documentation,  and
that  credit  is  given  to  Carnegie  Mellon  University  in all
publications reporting on direct or indirect use of this code  or
its derivatives.

RVM  IS  AN  EXPERIMENTAL  SOFTWARE  PACKAGE AND IS KNOWN TO HAVE
BUGS, SOME OF WHICH MAY  HAVE  SERIOUS  CONSEQUENCES.    CARNEGIE
MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.
CARNEGIE MELLON DISCLAIMS ANY  LIABILITY  OF  ANY  KIND  FOR  ANY
DAMAGES  WHATSOEVER RESULTING DIRECTLY OR INDIRECTLY FROM THE USE
OF THIS SOFTWARE OR OF ANY DERIVATIVE WORK.

Carnegie Mellon encourages (but does not require) users  of  this
software to return any improvements or extensions that they make,
and to grant Carnegie Mellon the  rights  to  redistribute  these
changes  without  encumbrance.   Such improvements and extensions
should be returned to Software.Distribution@cs.cmu.edu.
```

## Differences from Previous Versions

<!--
### Release (RVM 1.4 - 23 September 1997)

This release fixs a log wrapping bugs.  The log format is different to
that of previous release.  Therefore, *you must reinitialize your
log to make use of the new format*.  To preserve your data in log,
we provide a utility program which can recognize the old log format
and simply truncate it (i.e., apply for transaction records in the log
to the data segment).  The rdsinit interface is imporved.  It can now
be called with a -f switch (for firm) that will not ask any
interactive confirmation.  This make it handy to be used by script.
-->

### Release (RVM 1.3 - 29 June 1994)

This release corrects some error reporting bugs in `rvmutl` and the
segment loader.  No reinitialization of log files is necessary.  All
that is required to use this version is to recompile and relink your
applications after importing and installing RVM.

### Release (RVM 1.3 - 25 April 1994)

This release corrects a few small bugs and synchronizes the export and
Coda versions.  Re-initialization of your log files is recommended.

To reinitialize log files, for each existing log file (or partition),
use your present version of `rvmutl` to force any remaining records to
be reflected in the modified segments, and then reinitialize the log
with the new version of `rvmutl`:

``` sh
.../RVM1.2/bin/rvmutl        # RVM 1.2
* open_log <log file name>
* recover
* quit
```

``` sh
.../RVM1.3/bin/rvmutl        # RVM 1.3
* init_log <log file name> <size>
* quit
```

### Release (RVM 1.3 - 4 April 1994)

This release contains a more complete verion of the manual and corrects a bug
in `rvm_basher` that caused reports of virtual memory not matching the disk.
The problem was in the test program itself, not the RVM library.

Other changes are addition of error checking in the segment loader
and RDS initialization functions.  Also, the utility `rdsinit` has
been made somewhat more forgiving.  You can now enter lengths in
decimal as well as octal and hexadecimal.  The values input are
checked for minimal agreement with the requirements of RDS and a size
check on the file being created is made to insure that the regions
declared will fit in the file.

No recompilation or log re-initialization is necessary if you are
upgrading from the 14 March 1994 release.  Those who are using the
pre-public release, RVM 1.2, should follow the instructions below.

### Release (RVM 1.3 - 14 March 1994)

This release was the first public release, but for those presently using
the informal release, RVM 1.2, recompilation of modules using RVM and
re-initialization of the log is necessary.  To reinitialize the log,
use the procedure specified above.

## Importing RVM

To import RVM, use FTP to get the file RVM.TAR into the directory from
which you wish to build RVM.  Use the CMU Computer Science FTP server:

``` sh
cd <your directory>
ftp FTP.CS.CMU.EDU
Name: anonymous
password: <userid>@<host>
cd project/rvm-releases
get README               # this text
get rvm_manual.ps        # RVM manual (PostScript)
binary                   # required for tar files
get RVM1.3.TAR           # packed source files
quit
```

You should also send mail to [rvm@cs.cmu.edu](mailto:rvm@cs.cmu.edu) to be
placed on a mailing list for future updates.

Then unpack the files:

``` sh
tar xf RVM1.3.TAR
```

At this point, you can delete the RVM1.3.TAR file to recover disc space.

## Machine/OS Dependent Fixes

If you are building for any of the following machine and/or types, you
must make the following modifications before continuing the installation.
If your machine/OS is not mentioned, no special action is needed.

### OSF/1

Delete the following files:

``` text
.../RVM1.3/Makeconf
.../RVM1.3/rvm/Makeconf
.../RVM1.3/rds/Makeconf
.../RVM1.3/seg/Makeconf
.../RVM1.3/tests/Makeconf
.../RVM1.3/bench-SOSP93/Makeconf
```

### HP 9000 "snakes"

make doesnt support the include directive, so you will have to modify
the Makefiles.  In the following files, replace the include command
after the "get basic definitions" with the definitions and macros in
the build_defs file which will be found in the RVM directory created
by unpacking the tar file:

``` text
.../RVM1.3/rvm/Makefile
.../RVM1.3/rds/Makefile
.../RVM1.3/seg/Makefile
.../RVM1.3/tests/Makefile
.../RVM1.3/bench-SOSP93/Makefile
```

## Installing RVM

Execute the following commands:

``` sh
cd RVM1.3
make initialize
```

The above sequences will create the necessary directory structure
rooted as `RVM1.3` in the directory where the tar file
was unpacked, and compile the system.  When complete, you will have
the following directories under RVM1.3:

- **lib**: repository for the RVM, RDS and SEG libraries.
- **bin**: repository for RVM-related programs, rvmutl, rdsinit, and the test programs.
- **include**: repository for header files for RVM subsystems.
- **rvm**: source code for RVM.
- **rds**: source code for RDS, a dynamic allocator.
- **seg**: source code for SEG, a loader for RVM segments.
- **tests**: source code and test files for RVM.
- **bench-SOSP93**: source code for the benchmark used in the SOSP-93 paper[@satya93]
- **OBJS**: (Mach only) directory structure for machine-specific build areas,
  normally cleared after the initial build process.

These directories and files are fully built (for Mach) in the RVM1.3
directory on the FTP server, so you can see what you should get if
there is trouble.

The lib, bin, and include directories are the interface for
application build procedures.  References to them will get the correct
headers, libraries, and utility programs.  They are particularly
important under Mach since lib and bin are symbolic links to machine
dependent files.  Under Unix, they are directories with the actual
files; no support is provided for compilation for multiple machine
types.  Also, the build procedure under Unix leaves all derived files
in the same directories as the sources since the OBJS directory
structure is not built.  The structure used by the Mach build process
could also be used for Unix by modifying the Makefiles to use the
machine specifier used on your system to replace the \@sys construct
used in Mach.

### The Makefiles

The Makefiles included are intended to both install the system and
help with the building of customized versions.  The process can be
controlled from two levels: the top level, at the RVM1.3 directory, for
builds of all the subsystems.  The second level is builds of a
specific subsystem from its source directory.  From the top level, the
following build options are available:

- **make initialize**: creates directory structures for derived files and then
  compiles all libraries, utility and test programs.  This should be done only
  once.
- **make install**: does a complete recompilation and installation.  If the
  necessary directories are not present, they will be built.  All existing
  derived files are removed before the build so that a complete rebuild is guaranteed.
- **make release**: builds all subsystems from existing derived files, and
  copies the interface files to the appropriate directories.  Intended for a
  release after work has been done on one or more subsystems.
- **make all**: builds all subsystems from existing derived files, but does not
  copy files to the interface directories.
- **make clean**: removes all derived files from the subsystem directories and
  OBJS, if it is built.  The `lib` and `bin` directories are not affected.
  This is recommended to recover disk space after the initial build of RVM if
  you do not plan on making modifications to RVM.
- **make {rvm, rds, seg, tests, bench-SOSP93}**: builds the specified subsystem.

From the subsystem level, the install, reinstall, all, and clean
options are also available, and do their respected operation for only
that subsystem.  Further options are also available to create
specific libraries, utilities, and test programs.  These can be
determined by examining the subsystem Makefiles.

The version of Make used in Mach 2.6 will automatically include a file
called Makeconf if it exists.  This is used for common configuration
macros.  Since Unix Make doesnt support this, but does have an include
facility, most configuration macros are actually put in the file
`RVM1.3/build_defs`, which is included are necessary.  The Makeconf
files contain Mach specific things only.  To allow the same Makefiles to be
used for both operating systems, most directories are referred to by
macro names which will evaluate appropriately for the system.  This is
done using only the most basic Make features in the hope that the
build will be as portable as possible.  One artifact of this method is
that the installation will produce a number of error messages followed
by messages of the form `"*** Exit 1 (ignored)"` as the inappropriate
commands fail.  Just note that this is expected behavior.

There are a number of other build options that are only used for the
Coda File System.  These should be ignored since they require many
files not included in the RVM distribution.

### Threads

Under Mach, RVM is internally multi-threaded and expects to be linked
with the Cthreads package.  Either the Mach threads or coroutine
libraries can be used.  Coroutines are recommended while debugging
since program behavior can be repeatable.

Under Unix, RVM is at this time single threaded.  To allow RVM to
compile, symbolic links are created for the header file cthreads.h to
the file dummy_cthreads.h.  No thread library will be required at link
time.  If you have a thread package that you would like to try to use,
the file rvm_lwp.h can be used as a model of how to create a header
file for another thread package.  This not recommended for the faint
of heart!

## Testing RVM

This distribution of RVM includes two test programs: testrvm, a basic
"sanity checker", and rvm_basher, a stress test.  You should run these
after installing RVM.  Running the simple test involves two steps:
creating a log file with rvmutl, and then running the test.  Its most
easily done from the `RVM1.3/tests` directory, but if you copy the
map_*_files and the t*_chk_files to any other place, you can
run it there.  The procedure is:

``` sh
.../RVM1.3/bin/rvmutl
* init_log log_file 10k
* quit

.../RVM1.3/bin/testrvm
```

It will then ask three questions, all of which you should simply enter
the default "No" by hitting return.  It will do some basic
transactions and compare virtual memory with the map_chk_file for
correct operation.  It will report correct operation.  Any other
termination is an indication of trouble.

rvm_basher is a much more thorough test and should be run for several
hours (we have fixed bugs that would only show up after running it for
many days on Pentium 90 computers!).  The program requires that a data
segment be built as well as a log file.  The tests will randomly
allocate, modify, and deallocate recoverable memory, periodically
stopping to check virtual memory against the data segment.  Any
discrepancy will be reported and the program will stop.  Otherwise it
will run until you stop it, given the parameters supplied in this
distribution.

rvm_basher can also be used a a sample RVM application since it uses
most of the RVM library and also the dynamic allocator, RDS, and
segment loader.  The setup procedure for running it is similar to what
most applications will require, although their heap and static sizes
wiil most likely differ.

First, cd to a directory in which you can build about 2M of files.
Create a log file using rvmutl as in the the previous test:

``` sh
.../RVM1.3/bin/rvmutl
* init_log basher_log 600k
* sizeof page
* quit
```

To prepare a data segment, first load
`...RVM1.3/bin/rvm_basher` and run the following commands:

``` sh
.../RVM1.3/bin/rvm_basher
* log basher_log
* show_break
* run
```

The program will load and print the current break point (with
sufficient space added to compensate for internal requirements).
This will become the load point for the data segment.  Application
programs will also have to determine their break point after
initialization.  If you plan to use pointers in recoverable storage,
be sure to add enough to the breakpoint to allow growth of your
application.  Otherwise you will have to modify the pointers or
re-initialize recoverable storage to allow the break point to be moved.

Next, create a data segment with an RDS heap using the rdsinit utility.
The static length must be the size of a page on your machine as
indicated by rvmutl when you created the log.  (Note: all mappings
must be integral page sized in RVM; your application can use as many
pages as need in both the heap and static regions -- the basher needs
only the minimum.)

``` sh
.../RVM1.3/bin/rdsinit basher_log basher_data
Enter the length of the file/device basher_data: 1000000
   <rdsinit will zero the file and initialize RVM>
starting address of rvm: <something much above the break point>
heap len: 0xf0000
static len: <static len>
nlists: 100
chunksize: 128
```

**NOTE:** On Linux Sparc, you must give page size 8192, not 4096.

You are now ready to run the basher:

``` sh
.../RVM1.3/bin/rvm_basher < .../RVM1.3/tests/basher_parms
```

The file `.../RVM1.3/tests/basher_parms` contains all the parameters to run a
thorough test of RVM and its subsystems.  As indicated earlier, allowing it to
run for at least several hours is recommended.

## The Benchmark

The benchmark program used for the SOSP 1993 paper, *Lightweight
Recoverable Virtual Memory*[@satya93], is included in this
distribution.  It is not known to work on Unix or on versions of Mach
outside the Computer Science Dept. at CMU.  Unlike RVM, it is
dependent on C++, and may have dependencies on the exact compiler
version.  The version of C++ used at CMU is AT&T C++ Version It is
included as documentation for the methods used to produce the data for
the paper and most likely will not work as distributed on your
machine.

One known problem is that the CMU CS C++ systems use <libc.h>
instead of <stdlib.h> which does not work correctly here.
Before attempting to compile the benchmark, you will have to change
this, assuming your <stdlib.c> works.  The inclusion of
<mach.h>, which is required for compilation, will also cause
trouble on Unix systems.  Also, attempts to use other C++ systems have
produced multiple definition errors for some of the standard functions
such perror.

Because of these problems, no attempt is made to compile the
benchmark.  However, a Makefile that works at CMU is included as a
starting point if you wish to try building it.

## Contact with RVM Maintainers

Email should be sent to [rvm@cs.cmu.edu](mailto:rvm@cs.cmu.edu).  If you send
mail to this account when you import RVM, you will be put on a mailing list of
future upgrades and bug fixes.

Please understand that if you modify RVM, or any other parts of this
distribution, we cannot provide assistance in debugging your
modifications.
