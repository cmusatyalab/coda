# rvm_initialize

## NAME

rvm_initialize - RVM initialization

## SYNOPSIS

```c
#include "rvm.h"

typedef struct
    {
    char            *log_dev;
    long            truncate;
    rvm_length_t    recovery_buf_len;
    rvm_length_t    flush_buf_len;
    rvm_length_t    max_read_len;
    rvm_length_t    flags;
    }
rvm_options_t;

rvm_return_t rvm_initialize (version, options)
rvm_return_t RVM_INIT (options)

char          *version;   /* RVM library version */
rvm_options_t *options;   /* pointer to a RVM options record */
```

## DESCRIPTION

rvm_initialize must be called once to initialize the RVM library, and
must be called before any other RVM function.
Without successful initialization, any call to RVM functions except
rvm_initialize will result in the error return RVM_EINIT.
Multiple calls to rvm_initialize are ignored.
However, rvm_initialize cannot be used to restart RVM after
rvm_terminate has been called.

The version parameter is used to detect version skews.
The string constant RVM_VERSION from rvm.h is passed to
rvm_initialize, and compared with the value from the header file
the library was compiled with. If they are different,
RVM_EVERSION_SKEW is returned and RVM will not initialize.

RVM operation option choices are specified with the options parameter.
A options record created with typedef rvm_options_t must be used.
RVM_EOPTIONS will be returned if options does not point to a valid record.

The name of the log file must be specified in the options record.
If either the name is not provided, or the file cannot be opened and
initialized, RVM_ELOG will be returned. If options other than the log
file are not specified, defaults will be assumed. These can be changed
later with rvm_set_options.

rvm.h defines a macro, RVM_INIT, with a single parameter for the options,
that calls rvm_initialize and automatically passes RVM_VERSION.
This is the best way to initialize RVM.

## DIAGNOSTICS

- **RVM_SUCCESS**: success
- **RVM_EVERSION_SKEW**: RVM library version skew
- **RVM_ELOG**: invalid log file
- **RVM_EIO**: I/O or kernel error, errno has code number
- **RVM_EOPTIONS**: invalid RVM options record or pointer
- **RVM_ENO_MEMORY**: heap exhausted
- **RVM_EINIT**: RVM previously terminated
- **RVM_EPORTABILITY**: internal portability problem, see system maintainer

## SEE ALSO

[rvm_terminate](rvm_terminate.3.md), [rvm_set_options](rvm_set_options.3.md), [rvm_query](rvm_query.3.md)

## AUTHOR

Hank Mashburn
