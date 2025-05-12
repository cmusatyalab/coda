# LWP Library

The LWP library implements primitive functions providing basic facilities that
enable procedures written in C, to proceed in an unsynchronized fashion.  These
separate threads of control may effectively progress in parallel, and more or
less independently of each other.  This facility is meant to be general purpose
with a heavy emphasis on simplicity.  Interprocess communication facilities can
be built on top of this basic mechanism, and, in fact, many different IPC
mechanisms could be implemented.  The RPC2 remote procedure call library is one
such IPC mechanism.

The LWP library makes the following key design choices:

- The library should be small and fast;
- All processes are assumed to be trustworthy -- processes are not protected
  from each others actions;
- There is no time slicing or preemption -- the processor must be yielded
  explicitly.

In order to set up the environment needed by the lightweight process support, a
one-time invocation of the `LWP_Init` function must precede the use of the
facilities described here.  The initialization function carves an initial
process out of the currently executing C procedure.  The process id of this
initial process is returned as the result of the `LWP_Init` function.  For
symmetry a `LWP_TerminateProcessSupport` function may be used explicitly to
release any storage allocated by its initial counterpart.  If used, it must be
issued from the process created by the `LWP_Init` function.

Upon completion of any of the lightweight process functions, an integer value
is returned to indicate whether any error conditions were encountered.

Macros, typedefs, and manifest constants for error codes needed by the
lightweight process mechanism reside in the file
[`<lwp/lwp.h>`][lwp_h]:octicons-link-external-16:. A process is identified by
an object of type `PROCESS`, which is defined in the include file.

[lwp_h]: https://github.com/cmusatyalab/coda/blob/master/lib-src/lwp/include/lwp/lwp.h

The process model supported by the operations described here is based on a
non-preemptive priority dispatching scheme. (A priority is an integer in the
range `[0..LWP_MAX_PRIORITY]`, where 0 is the lowest priority.) Once a given
lightweight process is selected and dispatched, it remains in control until it
voluntarily relinquishes its claim on the CPU.  Relinquishment may be either
explicit (`LWP_DispatchProcess`) or implicit (through the use of certain other
LWP operations).  In general, all LWP operations that may cause a higher
priority process to become ready for dispatching, preempt the process
requesting the service.  When this occurs, the priority dispatching mechanism
takes over and dispatches the highest priority process automatically.  Services
in this category (where the scheduler is guaranteed to be invoked in the
absence of errors) are

- `LWP_CreateProcess`
- `LWP_WaitProcess`
- `LWP_MwaitProcess`
- `LWP_SignalProcess`
- `LWP_DispatchProcess`
- `LWP_DestroyProcess`

The following services are guaranteed not to cause preemption (and so may be
issued with no fear of losing control to another lightweight process):

- `LWP_Init`
- `LWP_NoYieldSignal`
- `LWP_CurrentProcess`
- `LWP_StackUsed`
- `LWP_NewRock`
- `LWP_GetRock`

The symbol `LWP_NORMAL_PRIORITY` provides a good default value to use for
process priorities.

## A word about initialization

The LWP, IOMGR, and FastTime components of the LWP library have routines that
perform global initialization for the package.  Each of these routines may be
called more than once, and only the parameters from the first invocation will
be used.  In addition, each routine calls any of the others that it needs for
proper operation.

The result is that if you only use one package directly, you need only call the
initialization routine for that package.  You may call the initialization
routines for other packages anyway in order to set the initialization
parameters yourself.  If you wish to initialize all of these packages yourself,
you must call the initialization routines in this order: FastTime, LWP, IOMGR.

Only after the LWP components have been initialized can you initialize
dependent libraries like RPC2 and RVM.

In contrast, the Preemption component initialization routine may be called
multiple times to change the value of the preemption time slice.

The Lock and Timer component have initialization routines that initialize
objects instead of global data.  The only restriction on the order of the
initialization calls to these components is that calls to `TM_Init` must
follow your call to `FT_Init`, if you have one.

## A Simple Example

``` c
#include <lwp/lwp.h>

static void read_process(int *id)
{
    LWP_DispatchProcess();  /* Just relinquish control for now */

    for (;;) {
        /* Wait until there is something in the queue */
        while (empty(q))
            LWP_WaitProcess(q);

        /* Process queue entry */

        LWP_DispatchProcess();
   }
}

static void write_process()
{
    ...

    /* Loop & write data to queue */
    for (mesg = messages; *mesg != 0; mesg++) {
        insert(q, *mesg);
        LWP_SignalProcess(q);
    }
}

int main (int argc, char **argv)
{
    PROCESS *id;
    int i;

    LWP_Init(LWP_VERSION, 0, &id);

    /* Now create readers */
    for (i = 0; i < nreaders; i++)
        LWP_CreateProcess(read_process, STACK_SIZE, 0, i, "Reader", &readers[i]);

    LWP_CreateProcess(write_process, STACK_SIZE, 1, 0, "Writer", &writer);

    /* Wait for processes to terminate */
    LWP_WaitProcess(&done);

    for (i = nreaders-1; i >= 0; i--)
        LWP_DestroyProcess(readers[i]);

    return 0;
}
```

## LWP Runtime Calls

### LWP\_Init

``` c
int LWP_Init(
    in char *VersionId,
    in int priority,
    out PROCESS *pid
);
```

Initializes the LWP library.  In addition, this routine turns the current
thread of control into the initial process with the specified priority.
The process id of this initial process will be returned in parameter pid.
This routine must be called to ensure proper initialization of the LWP
routines.  This routine will not cause the scheduler to be invoked.

**Parameters**

:   `VersionId`

    :   Set this to the constant `LWP_VERSION`.  The current value of this string
        constant must be identical to the value at the time the client runtime
        system was compiled.

    `priority`

    :   Priority at which initial process is to run.

    `pid`

    :   The process id of the initial process will be returned in this parameter.

**Completion Codes**

:   `LWP_SUCCESS`

    :   All went well.

    `LWP_EBADPRI`

    :   Illegal priority specified (< 0 or too large).

### LWP\_TerminateProcessSupport

``` c
int LWP_TerminateProcessSupport();
```

This routine will terminate the LWP process support and clean up by freeing
any auxiliary storage used.  This routine must be called from within the
procedure and process that invoked `LWP_Init`.  After
`LWP_TerminateProcessSupport` has been called, `LWP_Init` may be called
again to resume LWP process support.

### LWP\_CreateProcess

``` c
int LWP_CreateProcess(
    in int (*ep) (),
    in int stacksize,
    in int priority,
    in char *parm,
    in char *name,
    out PROCESS *pid
);
```

This routine is used to create and mark as runnable a new light-weight process.
This routine will cause the scheduler to be called.  Note that the new process
will begin execution before this call returns only if the priority of the new
process is greater than or equal to the priority of the creating process.

**Parameters**

:   ep

    :   This is the address of the code that is to execute the function of this
        process.  This parameter should be the address of a C routine with a
        single parameter.

    stacksize

    :   This is the size (in bytes) to make the stack for the newly-created
        process.  The stack cannot be shrunk or expanded, it is fixed for the
        life of the process.

    priority

    :   This is the priority to assign to the new process.

    parm

    :   This is the single argument that will be passed to the new process.
        Note that this argument is a pointer and, in general, will be used to
        pass the address of a structure containing further "parameters".

    name

    :   This is an ASCII string that will be used for debugging purposes to
        identify the process.  The name may be a maximum of 32 characters.

    pid

    :   The process id of the new process will be returned in this parameter.

**Completion Codes**

:   `LWP_SUCCESS`

    :   Process created successfully.

    `LWP_ENOMEM`

    :   Not enough free space to create process.

    `LWP_EBADPRI`

    :   Illegal priority specified (< 0 or too large).

    `LWP_EINIT`

    :   `LWP_Init` has not been called.

### LWP\_DestroyProcess

``` c
int LWP_DestroyProcess(
    in PROCESS pid
);
```

This routine will destroy the specified process.  The specified process will be
terminated immediately and its internal storage will be freed.  A process is
allowed to destroy itself (of course, it will only get to see the return code
if the destroy fails).  Note a process may also destroy itself by executing a
`return` from the C routine.  This routine calls the scheduler.

**Parameters**

:   `pid`

    :   The process id of the process to be destroyed.

**Completion Codes**

:   `LWP_SUCCESS`

    :   Process destroyed successfully.

    `LWP_EINIT`

    :   `LWP_Init` has not been called.

### LWP\_WaitProcess

``` c
int LWP_WaitProcess(
    in char *event
);
```

Wait for event.  This routine will put the calling process to sleep until
another process does a call of `LWP_SignalProcess` or `LWP_NoYieldSignal` with
the specified event.  Note that signals of events are not queued: if a signal
occurs and no process is woken up, the signal is lost.  This routine invokes
the scheduler.

**Parameters**

:   `event`

    :   The event to wait for.  This can be any memory address.  But, 0 is an
        illegal event.

**Completion Codes**

:   `LWP_SUCCESS`

    :   The event has occurred.

    `LWP_EINIT`

    :   `LWP_Init` has not been called.

    `LWP_EBADEVENT`

    :   The specified event was illegal (0).

### LWP\_MwaitProcess

``` c
int LWP_MwaitProcess(
    in int wcount,
    in char *evlist
);
```

Wait for a specified number of a group of signals. This routine allows a
process to wait for wcount signals of any of the signals in evlist.  Any number
of signals of a particular event is only counted once.  The scheduler will be
invoked.

**Parameters**

:   `wcount`

    :   Is the number of events that must be signaled to wake up this process.

    `evlist`

    :   This a null-terminated list of events (remember that 0 is not a legal
        event).  There may be at most `LWP_MAX_EVENTS` events

**Completion Codes**

:   `LWP_SUCCESS`

    :   The specified number of appropriate signals has occurred.

    `LWP_EBADCOUNT`

    :   There are too few events (0) or wcount > the number of events in evlist.

    `LWP_EINIT`

    :   `LWP_Init` has not been called.

### LWP\_SignalProcess

``` c
int LWP_SignalProcess(
    in char *event
);
```

This routine causes event to be signaled.  This will mark all processes waiting
for only this event as runnable.  The scheduler will be invoked.  Signals are
not queued: if no process is waiting for this event, the signal will be lost
and `LWP_ENOWAIT` will be returned.

**Parameters**

:   `event`

    :   The event to be signaled.  An event is any memory address except 0.

**Completion Codes**

:   `LWP_SUCCESS`

    :   The signal was a success (a process was waiting for it).

    `LWP_EBADEVENT`

    :   The specified event was illegal (0).

    `LWP_EINIT`

    :   `LWP_Init` was not called.

    `LWP_ENOWAIT`

    :   No process was waiting for this signal.

### LWP\_NoYieldSignal

``` c
int LWP_NoYieldSignal(
    in char *event
);
```

This routine causes event to be signaled.  This will mark all processes waiting
for only this event as runnable.  This call is identical to `LWP_SignalProcess`
except that the scheduler will not be invoked -- control will remain with the
signalling process.  Signals are not queued: if no process is waiting for this
event, the signal will be lost and `LWP_ENOWAIT` will be returned.

**Parameters**

:   `event`

    :   The event to be signaled.  An event is any memory address except 0.

**Completion Codes**

:   `LWP_SUCCESS`

    :   The signal was a success (a process was waiting for it).

    `LWP_EBADEVENT`

    :   The specified event was illegal (0).

    `LWP_EINIT`

    :   `LWP_Init` was not called.

### LWP\_DispatchProcess

``` c
int LWP_DispatchProcess();
```

This routine is a voluntary yield to the LWP scheduler.

**Completion Codes**

:   `LWP_SUCCESS`

    :   All went well,

    `LWP_EINIT`

    :   `LWP_Init` has not been called.

### LWP\_CurrentProcess

``` c
int LWP_CurrentProcess(
    out PROCESS *pid
);
```

Get the current process id.  This routine will place the current process id in
the parameter pid.

**Parameters**

:   `pid`

    :   The current process id will be returned in this parameter.

**Completion Codes**

:   `LWP_SUCCESS`

    :   The current process id has been returned.

    `LWP_EINIT`

    :   `LWP_Init` has not been called.

### LWP\_StackUsed

``` c
int LWP_StackUsed(
    in PROCESS pid,
    out int *max,
    out int *used
);
```

This routine returns the amount of stack space allocated to the process and the
amount actually used by the process so far.  It works by initializing the stack
to a special pattern at process creation time and checking to see how much of
the pattern is still there when `LWP_StackUsed` is called.  The stack of the
process is only initialized to the special pattern if the global variable
`lwp_stackUseEnabled` is true when the process is created.  This variable is
initially true.  If `lwp_stackUseEnabled` was false at the time the process was
created, then `*used` will be set to zero and the routine will return
`LWP_NO_STACK`.

**Parameters**

:   `pid`

    :   The target process.

    `max`

    :   Max stack size given at process creation time.

    `used`

    :   Stack used so far.

**Completion Codes**

:   `LWP_SUCCESS`

    :   No problem.

    `LWP_NO_STACK`

    :   Stack counting was not enabled for this process.

### LWP\_NewRock

``` c
int LWP_NewRock(
    in int Tag,
    in char *Value
);
```

Find a rock under which private information can be hidden.  The rock is exactly
what its name implies: a place to squirrel away application-specific
information associated with an LWP.  The Tag is any unique integer.  Users of
the LWP library must coordinate their choice of Tag values.  Note that you
cannot change the value associated with Tag.  To obtain a mutable data
structure use one level of indirection.

**Parameters**

:   `Tag`

    :   A unique integer identifying this rock.

    `Value`

    :   A value (usually a pointer to some data structure) to be associated with
        the current LWP and identified by Tag.

**Completion Codes**

:   `LWP_SUCCESS`

    :   No problem.

    `LWP_EBADROCK`

    :   Rock called Tag already exists for this LWP.

    `LWP_ENOROCKS`

    :   All rocks are in use.

### LWP\_GetRock

``` c
int LWP_GetRock(
    in int Tag,
    out char **Value
);
```

Recovers information hidden by a `LWP_NewRock` call.

**Parameters**

:   `Tag`

    :   Rock under which to look.

    `Value`

    :   The current value (usually a pointer to some data structure) hidden under
        this rock.

**Completion Codes**

:   `LWP_SUCCESS`

    :   Value has been filled.

    `LWP_EBADROCK`

    :   Specified rock does not exist.

## Acknowledgements

The original design and implementation of LWP was done by Larry Raper.  Its
documentation descends from a manual by Jonathan Rosenberg and Larry Raper,
later e:octicons-link-external-16:xtended by David Nichols and M. Satyanarayanan.
