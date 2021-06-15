# IOMGR package

The IOMGR package allows light-weight processes to wait on various Unix
events.  `IOMGR_Select` allows a light-weight process to wait on the same
set of events that the Unix `select` call waits on.  The parameters to
these routines are the same.  `IOMGR_Select` puts the caller to sleep
until no user processes are active.  At this time the IOMGR process, which
runs at the lowest priority, wakes up and coaleses all of the select request
together.  It then performs a single `select` and wakes up all processes
affected by the result.

The `IOMGR_Signal` call allows a light-weight process to wait on delivery
of a Unix signal.  The IOMGR installs a signal handler to catch all
deliveries of the Unix signal.  This signal handler posts information about
the signal delivery to a global data structure.  The next time that the
IOMGR process runs, it delivers the signal to any waiting light-weight
processes.

## Key Design Choices

- The meanings of the parameters to `IOMGR_Select`, both before and after the
  call, should be identical to those of the Unix `select`.
- A blocking select should only be done if no other processes are runnable.

## A Simple Example

``` c
void rpc2_SocketListener();
{
    int ReadfdMask, WritefdMask, ExceptfdMask, rc;
    struct timeval *tvp;

    while (TRUE) {
        ...
        ExceptfdMask = ReadfdMask = (1 << rpc2_RequestSocket);
        WritefdMask = 0;
        rc = IOMGR_Select(8*sizeof(int), &ReadfdMask, &WritefdMask, &ExceptfdMask, tvp);

        switch (rc) {
            case 0:  /* timeout */
                continue;  /* main while loop */

            case -1:  /* error */
                SystemError("IOMGR_Select");
                exit (-1);

            case 1:  /* packet on rpc2_RequestSocket */
                ... process packet ...
                break;

            default:  /* should never occur */
        }
    }
}
```

## IOMGR Primitives

### IOMGR\_Initialize

``` c
int IOMGR_Initialize();
```

Initialize the IOMGR package.  Its main task is to create the IOMGR process,
which runs at priority 0, the lowest priority.  The remainder of the processes
must be running at priority 1 or greater for the IOMGR package to function
correctly.

**Completion Codes:**

:   `LWP_SUCCESS`

    :   All went well.

:   `LWP_ENOMEM`

    :   Not enough free space to create the IOMGR process.

:   `-1`

    :   Something went wrong with other init calls.

### IOMGR\_Finalize

``` c
int IOMGR_Finalize();
```

Clean up after IOMGR package.  This call cleans up when the IOMGR package is
no longer needed.  It releases all storage and destroys the IOMGR process.

**Completion Codes:**

:   `LWP_SUCCESS`

    :   Package finalized okay.

### IOMGR\_Select

``` c
int IOMGR_Select(
    in int fds,
    in out int *readfds,
    in out int *writefds,
    in out int *exceptfds,
    in struct timeval *timeout
);
```

Perform an LWP select operation.  This function performs an LWP version of Unix
`select`.  The parameters have the same meanings as the Unix call.  However,
the return value will only be -1 (an error occurred), 0 (a timeout occurred),
or 1 (some number of file descriptors are ready).  If this is a polling select,
it is done and `IOMGR_Select` returns to the user with the results.  Otherwise,
the calling process is put to sleep.  If at some point, the IOMGR process is
the only runnable process, it will awaken and collect all select requests.  It
will then perform a single select and awaken those processes the appropriate
processes -- this will cause return from the affected `IOMGR_Select`'s.

**Parameters**

:   `fds`

    :   Maximum number of bits to consider in masks.

:   `readfds`

    :   Mask of file descriptors that process wants notification of when ready
        to be read.

:   `writefds`

    :   Mask of file descriptors that process wants notification of when ready
        to be written.

:   `exceptfds`

    :   Mask of file descriptors that process wants notification of when an
        exceptional condition occurs.

:   `timeout`

    :   Timeout for use on this select.

### IOMGR\_Signal

``` c
int IOMGR_Signal(
    in int signo,
    in char *event
);
```

Convert Unix signals to LWP signals.  This function associates an LWP signal
with a Unix signal.  Whenever the Unix signal is delivered to the process, the
IOMGR process will deliver an LWP signal to the event `event` via
`LWP_NoYieldSignal`, waking any light-weight processes waiting on that event.
Multiple deliveries of the signal may be coalesed into one LWP wakeup.  The
call to `LWP_NoYieldSignal` will happen synchronously.  It is safe for an LWP
to check for some condition and then go to sleep waiting for a Unix signal
without having to worry about delivery of the signal happening between the
check and the call to `LWP_WaitProcess`.

**Parameters**

:   `signo`

    :   The Unix signal number, as defined in `signal.h`.

:   `event`

    :   The light-weight process event that should be signaled whenever `signo`
        is delivered.

**Completion Codes**

:   `LWP_SUCCESS`

    :   No problems.

:   `LWP_EBADSIG`

    :   `signo` was out of range.

:   `LWP_EBADEVENT`

    :   `event` was zero.

### IOMGR\_CancelSignal

``` c
int IOMGR_CancelSignal(
    in int signo
);
```

Cancel association between Unix signal and LWP event.  This function cancels
the association of a Unix signal and an LWP event.  After calling this
function, the Unix signal `signo` will be handled however it was handled before
the corresponding call to `LWP_Signal`.

**Parameters**

:   `signo`

    :   The Unix signal number, as defined in `signal.h`.

**Completion Codes**

:   `LWP_SUCCESS`

    :   No problems.

:   `LWP_EBADSIG`

    :   `signo` was out of range or `LWP_Signal` has not been called on it.
