# Common Problems and Suggestions

!!! question "`ls /coda` fails."

    If the return code is either "Invalid Argument" or "No Device", venus was
    probably restarted recently and hasn't yet finished initializing.

    Solution: Wait a few minutes and try again.

!!! question "Newly created volume is a dangling link."

    If a newly created volume, when mounted for the first time, shows up as a
    dangling symbolic link (it looks inconsistent), the problem is that new
    databases have not been distributed to the other replication sites.

    Solution: Unmount the volume, wait five minutes and try again. You'll have
    to reinitialize the venus which failed to find the volume, since VLDB
    information cant be flushed.

!!! question "Venus Fails on Startup with "unable to unmount" message"

    There exists a process with an reference to a file in /coda. Its either a
    running process *cd*-ed into `/coda`

    To solve the problem, either get the process out of /coda (in case its a
    shell) or kill it the process. If you cant find the right process rebooting
    the workstation should clear it up.

!!! question "Venus error messages fail to print on console window of window manager"

    Use the *-console filename* option to **venus** on startup.

    This will force stderr messages to print to the specified file. Use
    `tail -f filename` in a window to observe any error messages.

!!! question "RVM gets Trashed"

    !!! warning "ABSOLUTE LAST RESORT"

        If RVM gets trashed -- it asserts, wont recover, there are many
        failures on salvage, dumpvm fails -- there are probably serious
        **venus** or **codasrv** bugs which must be fixed.

        Talk to Coda experts before continuing!!!

    Performing the following actions will result in the loss of ALL Coda file
    system data.

    - Reinitializing the RVM Data segment (i.e. with rdsinit)
    - Reinitializing the RVM Log (i.e. with rvmutl)
    - Rewriting or trashing the file containing the Log or Data segment.
