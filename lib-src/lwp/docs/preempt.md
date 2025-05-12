# Preemption package

The preemption package provides a mechanism by which control can pass between
light-weight processes without the need for explicit calls to
`LWP_DispatchProcess`.  This effect is achieved by periodically interrupting
the normal flow of control to check if other (higher priority) procesess are
ready to run.

The package makes use of the _interval timer_ facilities provided by 4.2BSD,
and so will cause programs that make their own use of these facilities to
malfunction.  In particular, use of _alarm(3)_ or explicit handling of
`SIGALRM` is disallowed.  Also, calls to _sleep(3)_ may return prematurely.

Care should be taken that routines are re-entrant where necessary.  In
particular, note that _stdio(3)_ is not re-entrant in general, and hence
light-weight processes performing I/O on the same FILE structure may function
incorrectly.

## Key Design Choices

- The package should not affect the nonpreemptive scheduling behaviour of
  processes which do not use it.
- It must be simple and **fast**, with a minimum of extra system overhead.
- It must support nested critical regions.
- Processes using the package are assumed to be _co-operating_.

## A Simple Example

``` c
#include <sys/time.h>
#include "preempt.h"

    ...
    struct timeval tv;

    LWP_Init(LWP_VERSION, ...);
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    PRE_InitPreempt(&tv);
    PRE_PreemptMe();
    ...
    PRE_BeginCritical();
    ...
    PRE_EndCritical();
    ...
    PRE_EndPreempt();
    ...
```

## Preemption Primitives

### PRE\_InitPreempt

``` c
int PRE_InitPreempt(
    in struct timeval *slice
);
```

Initialize the preemption package.  This routine must be called to initialize
the package (after the call to `LWP_Init`).

**Parameters**

:   `slice`

    :   The period of the implicit scheduler calls.  Use `NULL` to obtain a
        useful default.

**Completion Codes**

:   `LWP_SUCCESS`

    :   All went well.

:   `LWP_EINIT`

    :   `LWP_Init` was not called.

:   `LWP_ESYSTEM`

    :   A system call failed.

### PRE\_EndPreempt

``` c
int PRE_EndPreempt();
```

Finalize the preemption package.  No further preemptions will be made.  Note
that is not necessary to make this call before exit - it is provided only for
those applications that wish to continue after turning off preemptions.

**Completion Codes**

:   `LWP_SUCCESS`

    :   All went well.

:   `LWP_EINIT`

    :   `LWP_Init` was not called.

:   `LWP_ESYSTEM`

    :   A system call failed.

### PRE\_PreemptMe

``` c
PRE_PreemptMe();
```

This is a macro that marks the current process as a candidate for preemption.
It is erroneous to invoke `PRE_PreemptMe` if `LWP_Init` has not been called.

### PRE\_BeginCritical

``` c
PRE_BeginCritical();
```

This routine places the current LWP in a _Critical Section_.  Upon return,
involunatry preemptions of this process will no longer occur.  Note that this
is a macro and that `LWP_Init` must have been previously invoked.

### PRE\_EndCritical

``` c
PRE_EndCritical();
```

This routine leaves a critical section previously entered with
`PRE_BeginCritical`.  If involuntary preemptions were possible before the
matching `PRE_BeginCritical`, they are once again possible.  Note that this is
a macro and that `LWP_Init` must have been previously invoked.
