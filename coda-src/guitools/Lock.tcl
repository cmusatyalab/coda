proc InitLocks { } {
    global Locks

    set Locks(Output) 0
}

proc Lock { what } {
    global Locks

    while { $Locks($what) == 1 } {
	tkwait variable Lock($what)
    }

    set Locks($what) 1
}

proc UnLock { what } {
    global Locks

    set Locks($what) 0
}