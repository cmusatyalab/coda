# rvm_set_options

## NAME

rvm_set_options - change RVM options after initialization

## SYNOPSIS

```c
#include "rvm.h"

rvm_return_t rvm_set_options (options)
rvm_options_t *options;   /* pointer to a RVM options record */
```

## DESCRIPTION

rvm_set_options is used to set RVM operation option choices
after RVM has been initialized.
Only the log_dev option cannot be changed after initialization,
so if that field is set in the options record, RVM_ELOG will be returned.

## DIAGNOSTICS

- **RVM_SUCCESS**: success
- **RVM_ELOG**: log device name specified (cannot be changed)
- **RVM_EOPTIONS**: invalid RVM options record or pointer
- **RVM_ENO_MEMORY**: heap exhausted
- **RVM_EPORTABILITY**: internal portability problem, see system maintainer

## SEE ALSO

[rvm_initialize](rvm_initialize.3.md), [rvm_query](rvm_query.3.md)

## AUTHOR

Hank Mashburn
