# Fast Time package

The Fast Time package allows the caller to find out the current time of day
without incurring the expense of a kernel call.  It works by mapping the page
of the kernel that has the kernels time-of-day variable and examining it
directly.  Currently, this package only works on Suns.  You may call the
routines on other machines, but they will run more slowly.

The initialization routine for this package is fairly expensive since it does a
lookup of a kernel symbol via `nlist()`.  If you have a program which runs for
only a short time, you may wish to call `FT_Init` with the `notReally`
parameter true to prevent the lookup from taking place.  This is useful if you
are using another package that uses Fast Time (such as RPC2).

## Fast Time Primitives

### FT\_Init

``` c
int FT_Init(
    in int printErrors,
    in int notReally
);
```

Initialize the Fast Time package.  This call mmaps the kernel page with the
time of day variable.  If the routine returns -1, calls to `FT_GetTimeOfDay`
will still work properly, but will make a kernel call.

**Parameters**

:   `printErrors`

    :   Print error messages on stderr if something goes wrong.

:   `notReally`

    :   Don't really do the memory mapping.  Make `FT_GetTimeOfDay` make a
        kernel call.

**Completion Codes**

:   `0`

    :   No problems.

:   `-1`

    :   Error in initialization.

### FT\_GetTimeOfDay

``` c
int FT_GetTimeOfDay(
    out struct timeval *tv,
    out struct timezone *tz
);
```

This function has the same calling sequence as the kernel's `gettimeofday`
routine.  If the `tz` parameter is not zero, or if initialization failed, or if
the `notReally` parameter in the initialization was true, then this routine
calls `gettimeofday`.  Otherwise, it looks at the mapped kernel pages.

**Parameters**

:   `tv`

    :   Where to put the time of day.

:   `tz`

    :   Where to put the time zone information.

**Completion Codes**

:   `0`

    :   No problem.

:   `-1`

    :   Something went wrong.
