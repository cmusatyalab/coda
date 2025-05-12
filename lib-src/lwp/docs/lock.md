# Lock package

The lock package contains a number of routines and macros that allow C programs
that utilize the LWP abstraction to place read and write locks on data
structures shared by several light-weight processes.  Like the LWP library, the
lock package was written with simplicity in mind -- there is no protection
inherent in the model.

In order to use the locking mechanism for an object, an object of type `struct
Lock` must be associated with the object.  After being initialized, with a call
to `Lock_Init`, the lock is used in invocations of the macros `ObtainReadLock`,
`ObtainWriteLock`, `ReleaseReadLock` and `ReleaseWriteLock`.

The semantics of a lock is such that any number of readers may hold a lock.
But only a single writer (and no readers) may hold the lock at any time.  The
lock package guarantees fairness: each reader and writer will eventually have a
chance to obtain a given lock.  However, this fairness is only guaranteed if
the priorities of the competing processes are identical.  Note that no ordering
is guaranteed by the package.

In addition, it is illegal for a process to request a particular lock more than
once, without first releasing it.  Failure to obey this restriction may cause
deadlock.

## Key Design Choices

- The package must be simple and _fast_: in the case that a lock can be
  obtained immediately, it should require a minimum of instructions.
- All the processes using a lock are trustworthy.
- The lock routines ignore priorities.

## A Simple Example

``` c
#include <lwp/lock.h>

struct Vnode {
    ...
    struct Lock lock;  /* Used to lock this vnode */
    ...
}

#define READ  0
#define WRITE 1

struct Vnode *get_vnode(char *name, int how)
{
    struct Vnode *v;

    v = lookup (name);
    if (how == READ)
        ObtainReadLock(&v->lock);
    else
        ObtainWriteLock(&v->lock);
}
```

## Lock Primitives

### Lock\_Init

``` c
void Lock_Init(
    out struct Lock *lock
);
```

Initialize a lock. This routine must be called to initialize a lock before it
is used.

**Parameters**

:   `lock`

    :   The (address of the) lock to be initialized.

### ObtainReadLock

``` c
void ObtainReadLock(
    in out struct Lock *lock
);
```

Obtain a read lock.  A read lock will be obtained on the specified lock.  Note
that this is a macro and not a routine.  Thus, results are not guaranteed if
the lock argument is a side-effect producing expression.

**Parameters**

:   `lock`

    :   The lock to be read-locked.

### ObtainWriteLock

``` c
void ObtainWriteLock(
    in out struct Lock *lock
);
```

Obtain a write lock.  A write lock will be obtained on the specified lock.
Note that this is a macro and not a routine.  Thus, results are not guaranteed
if the lock argument is a side-effect producing expression.

**Parameters**

:   `lock`

    :   The lock to be write-locked.

### ReleaseReadLock

``` c
void ReleaseReadLock(
    in out struct Lock *lock
);
```

Release a read lock. The specified lock will be released.  This macro requires
that the lock must have been previously read-locked.  Note that this is a macro
and not a routine.  Thus, results are not guaranteed if the lock argument is a
side-effect producing expression.

**Parameters**

:   `lock`

    :   The lock to be released.

### ReleaseWriteLock

``` c
void ReleaseWriteLock(
    in out struct Lock *lock
);
```

Release a write lock.  The specified lock will be released.  This macro
requires that the lock must have been previously write-locked.  Note that this
is a macro and not a routine.  Thus, results are not guaranteed if the lock
argument is a side-effect producing expression.

**Parameters**

:   `lock`

    :   The lock to be released.

### CheckLock

``` c
int CheckLock(
    in struct Lock *lock
);
```

Check the status of a lock. This macro yields an integer that specifies the
status of the indicated lock.  The value will be -1 if the lock is write-locked
0 if unlocked, or a positive integer that indicates the numer of readers with
read locks.  Note that this is a macro and not a routine.  Thus, results are
not guaranteed if the lock argument is a side-effect producing expression.

**Parameters**

:   `lock`

    :   The lock to be checked.
