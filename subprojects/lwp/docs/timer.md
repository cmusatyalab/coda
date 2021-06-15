# Timer package

The timer package contains a number of routines that assist in manipulating
lists of objects of type `struct TM_Elem`.  `TM_Elem`s (timers) are assigned a
timeout value by the user and inserted in a package-maintained list.  The time
remaining to timeout for each timer is kept up to date by the package under
user control.  There are routines to remove a timer from its list, to return an
expired timer from a list and to return the next timer to expire.  This
specialized package is currently used by the IOMGR package and by the
implementation of RPC2.  A timer is used commonly by inserting a field of type
`struct TM_Elem` into a structure.  After inserting the desired timeout value
the structure is inserted into a list, by means of its timer field.

## A Simple Example

``` c
static struct TM_Elem *requests;

...
    TM_Init(&requests);  /* Initialize timer list */
    ...
    for(;;) {
        TM_Rescan(requests);  /* Update the timers */
        expired = TM_GetExpired(requests);
        if (expired == 0) break;
        ... process expired element ...
    }
```

## Timer Primitives

### TM\_Init

``` c
void TM_Init(
    out struct TM_Elem **list
);
```

Initialize a timer list.  The specified list will be initialized so that it is
an empty timer list.  This routine must be called before any other operations
are applied to the list.

**Parameters**

:   `list`

    :   The list to be initialized.

### TM\_Final

``` c
int TM_Final(
    out struct TM_Elem **list
);
```

Finalize a timer list.  Call this routine when you are finished with a timer
list and the list is empty.  This routine releases any auxiliary storage
associated with the list.

**Parameters**

:   `list`

    :   The list to be finalized.

**Completion Codes**

:   `0`

    :   ok.

:   `-1`

    :   `list` was 0 or `list` was never initialized.

### TM\_Insert

``` c
void TM_Insert(
    in struct TM_Elem *list,
    in out struct TM_Elem *elem
);
```

Insert an element into a timer list.  The specified element is initialized so
that the `TimeLeft` field is equal to the `TotalTime` field.  (The `TimeLeft`
field may be kept current by use of `TM_Rescan`.)  The element is then inserted
into the list.

**Parameters**

:   `list`

    :   The list into which the element is to be inserted.

    `elem`

    :   The element to be initialized and inserted.

### TM\_Rescan

``` c
void TM_Rescan(
    in struct TM_Elem *list
);
```

Update `TimeLeft` fields of entries on a timer list and look for expired
elements.  This routine will update the `TimeLeft` fields of all timers on the
list.  This is done by checking the time of day clock in Unix.  This routine
returns a count of the number of expired timers on the list.  This is the only
routine besides `TM_Init` that updates the `TimeLeft` field.

**Parameters**

:   `list`

    :   The list to be updated.

### TM\_GetExpired

``` c
struct TM_Elem *TM_GetExpired(
    in struct TM_Elem *list
);
```

Return an expired timer from a list.  The specified list will be searched and a
pointer to an expired timer will be returned.  `NULL` is returned if there are
no expired timers.  An expired timer is one whose `TimeLeft` field is less than
or equal to 0.

**Parameters**

:   `list`

    :   The list to be searched.

**Completion Codes**

:   `NULL`

    :   No expired timers.

### TM\_GetEarliest

``` c
struct TM_Elem *TM_GetEarliest(
    in struct TM_Elem *list
);
```

Return the earliest timer on a list.  This routine returns a pointer to the
timer that will be next to expire -- that with a smallest `TimeLeft` field.  If
there are no timers on the list, `NULL` is returned.

**Parameters**

:   `list`

    :   The list to be searched.

**Completion Codes**

:   `NULL`

    :   No timers on the list.

### TM\_eql

``` c
int TM_eql(
    in struct timeval *t1,
    in struct timeval *t2
);
```

See if 2 timevals are equal.  This routine returns 0 if and only if `t1` and
`t2` are not equal.

**Parameters**

:   `t1`, `t2`

    :   timevals.

**Completion Codes**

:   `0`

    :   `t1` and `t2` are not equal.
