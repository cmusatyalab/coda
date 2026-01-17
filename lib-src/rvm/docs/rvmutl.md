# rvmutl, the RVM Maintenance Utility

rvmutl is the maintenance utility for RVM.  Its principle
features are log creation and maintenance, but there are also status
and statistic display commands as well as printing commands for log
records.
A search facility for modifications to segments gains additional
functionality from the log by allowing its use as a debugging history.

rvmutl is included in the `bin` directory of the RVM
distribution directory.  It can be invoked with a single, optional
parameter, which is interpreted as the name of a log file to be opened.
If the file doesnt exist, rvmutl will ask if you wish to create
and initialize a log with the specified name.

rvmutl prompts for commands with a * prompt, after which commands
can be typed.  Most commands have one or more parameters, which are
separated by one or more spaces.
In general, command names can be abbreviated, provided that sufficient
characters are typed to uniquely specify a command.
To allow typing short-cuts, some non-unique abbreviations have defined
interpretations.
These are specified with the command documentation

<!--
Both the input and output streams can be redirected in a manner
similar to the redirection facilities of the C shell.
Redirection of output permits the output of commands to be captured in
files for printing or further processing.  This is often useful with the
statistics and log printing commands.

Redirection of input permits commands to be placed in files 
At present, this is of limited utility since the only initialization
performed by rvmutl is for log files.
However, as the initializations for the segment loader and allocator
are integrated into rvmutl, putting initialization sequences in
files will be valuable for automating application system builds.
-->

As an example, the following sequence will initialize the file
log_file to have one megabyte of log record space:

```
% rvmutl
* init_log log_file 1M
* quit
```

After this simple sequence, the log file is ready for use.

The following manual page describes the commands available in rvmutl.

- [rvmutl](manpages/rvmutl.1.md).
