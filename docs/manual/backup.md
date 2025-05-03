# Backup System

## Introduction: Design of the Coda Backup Subsystem

As the use of the Coda file system increased, the need for a reliable backup
storage system with a large capacity and a minimal loss of service became
apparent.  A one operation backup system was determined to be infeasible given
the volume of data in Coda, the nature of a distributed file-system, and the
long down-time that would normally be required to backup the system in one
operation.

 In order to meet the goals of high availability and reliability inherent in
Coda design and to make efficient use of backup hardware and materials, the
volume was chosen as the unit of data, and 24 hours was chosen as the time unit
for system management and administration.  The result of these design
considerations is a volume by volume backup mechanism that occurs in three
phases:

**Cloning**

The cloning phase consists of freezing the (replicated) volume, creating a
read-only clone of each of the replicas, and then unfreezing the volume.  This
allows mutating operations on the replicated volume to occur while maintaining
a snapshot to backup.  Once the cloning phase has been completed, normal
read-write services can be resumed without fear of data corruption due to
mutating operations on an active file system.

**Dumping to disk files on a backup spool machine.**

The dumping phase consists of converting the read-only volume clones to disk
images stored as regular disk files, on a spool machine.  A dump can either be
full (level 0), in which all files are dumped; or incremental (level 1 through
9), in which only those files or directories which have changed since the last
successful backup of the lower level are included in the dump. So a level 1
dump will include changes from last full dump, a level 2 - from last level 1
dump, and so on.  This allows for a system in which only a subset of volumes
need a full backup at anyone time (with incremental backups done between full
backups), thus reducing the amount of off-line storage and network bandwidth
needed at any one time.  However, it allows the re-creation of data a
granularity of 24 hours when combined with incremental dumps.

**Saving to media**

The last phase consists of writing all the dump files from the backed up
volumes dumped on local partitions to an archival media such as tape.  Any
standard backup system can be used for this phase.  At CMU, we use the BSD
**dump** and **restore** utilities to write and retrieve the disk images of
Coda volumes to tape.

Practically, this system has been implemented as series of tasks. The first two
tasks are carried out by the backup program the latter by a Coda independent
Perl script (tape.pl).

 1. Backup
     1. Create a read-only clone
     1. Dump the read-only clone to a local disk
     1. Backup the dumped data to a suitable archive media
 1. Restore
     1. Retrieve appropriate full and incremental dumps from the archive media.
     1. Merge the full and incremental dumps to the time line of restoration.
     1. Restore the fully integrated backup to the Coda file system.

Remember, in practice, many restores are a result of a user accidently deleting
or corrupting their own files.  In this case, users may use the the **cfs**
mechanism to retrieve files for the last 24 hour time line.  For example: `cfs
mkmount OldFiles u.hmpierce.0.backup`, will mount the hmpierce's user backup
volume from replica 0 to the _OldFiles_ mount point.  The file can then be
copied out (backup volumes are read-only). Only if restoration needs are older
than 24-hours or some catastrophic event outside of the users control occurs do
restores from tape normally need be done

Several tools have been developed to help in the creation, analysis, and
restoration of data backups.  Some of these tools have been developed by the
Coda team (those tools concerning Coda FS to local disk conversion) such as
**backup** and **tape.pl** (used to coordinate the efforts of **backup**,
**dump**, etc), others employ off-the-shelf software such as the traditional
UNIX **dump** or **tar** to transfer the disk images created from the dump
phase to the backup media.  Coda, however, provides a Perl script front-end to
**dump**.

## Installing a Coda Backup Coordinator Machine

The Coda Backup coordinator should be a trusted machine.  It should be able get
all the files that exist in the `/vice` subtree on the servers, although it is
not necessary to run a file-server on the backup coordinator (nor is it
recommended that the backup machine be a file-server).

Assuming the Backup Coordinator has been setup with an appropriate operating
system, the steps are as follows:

<!-- NEEDS SECTION RECOMMENDING DATA/LOCAL DISK SPACE RATIO -->
 1. On the Coda File Server designated as the SCM, verify that
    `/vice/db/dumplist` contains correct backup schedule for your volumes. If
    you were using **createvol_rep** for volume creation, you should have been
    prompted about the backup schedule for new volume.  The `dumplist` contains
    three fields: volume id specified as its hex value, the full/incremental
    backup schedule, and a comment which is generally the human readable volume
    name. For example:

        7f000001        IFIIIII         s:coda
        7f000002        IIIIIFI         u:satya
        7f000003        0325476         u:jan

    The first column specifies the volume id to be backed up, the second column
    specifies the backup schedule by the day of the week beginning Sunday, and
    the third column is a comment, usually the volume name in human readable
    form.  So, the volume id _7f000001_ is scheduled for a *F*ull backup every
    Monday, and *I*ncremental - Tuesday through Sunday and from the comment, we
    know this is a system volume called _coda_. Likewise, the second volume
    _7f000002_ is scheduled for a Full backup on Friday with incremental being
    done Saturday through Thursday and is a user volume called _satya_. The
    last entry shows that it is possible to numerically identify which
    dumplevels are to be made on each day.

 1. On the SCM, modify `/vice/db/vicetab` to indicate which host is acting as
    the backup coordinator and which partitions on the backup server are to be
    used by the backup coordinator to store the dump files.  On a triply
    replicated system, `vicetab` might look like this:

        tye             /vicepa         ftree  width=8,depth=5
        taverner        /vicepa         ftree  width=8,depth=5
        tallis          /vicepa         ftree  width=8,depth=5
        dvorak          /backup1        backup
        dvorak          /backup2        backup
        dvorak          /backup3        backup

    `vicetab`, in addition to listing information on the servers providing
    replicated data, must also include information on the backup coordinator
    with backup coordinator's name in the first column, the backup partitions
    in the second column, and the designation _backup_ in the third column.
    The 4th column is not used for the backup sub-system.  Please see the
    [vicetab.5](../manpages/vicetab.5.md) man page for additional information.

    The number of partitions available for dumping may be controlled by the
    system administrator.  Because the volume of data may be both large and
    variable, the **backup** program intelligently decides where to store
    individual dump files based on size across the specified backup partitions.
    The directories in the sample `vicetab`, are assumed to be separate local
    disk partitions.  An organized central symbolic link tree is created by the
    **backup.sh** script in the directory `/backup` that points to the actual
    files scattered across the `/backup1`, `/backup2`, and `/backup3` given in
    this example.

 1. On the Backup Coordinator, the backup binaries and shell scripts need to be
    installed.  The following directories should be have been created under
    `/vice` upon the installation of the Coda backup package:

        /backup
        /vice/backup
        /vice/backuplogs
        /vice/db
        /vice/vol
        /vice/lib
        /vice/spool
        /vice/srv

    The primary binaries that should be installed under `/usr/sbin` to get
    started are:

        backup
        backup.sh
        bldvldb.sh
        merge
        updateclnt
        updatesrv
        updfetch
        tape.pl

    Once it has been verified that the backup system is installed, the files
    `/vice/db/hosts`, `/vice/db/files` should be manually copied from the SCM
    to the same location on the Backup Coordinator.  These are needed the first
    time by the **updateclnt** daemon the when it runs.  Also, Coda currently
    relies on the BSD **dump** and **restore** commands to manipulate the
    tapes.  A copy of dump package should be installed on the backup
    coordinator.  BSD dump is available for all UNIX and UNIX like operating
    systems we have successfully run Coda on. Please check with your OS Vendor
    if you need help obtaining a copy.

Upon completion **backup** will print which volumes were successfully backed
up, the volumes on which backup failed, and those volumes which were not
specified for backup.

The **merge** program allows a system administrator to update the state seen in
a full dump by the partial state in an incremental dump.  This is useful, when
a user wishes to restore to a state that was captured by a full and some number
of incremental dumps, for instance, in the middle of the week.  **merge**
applies an incremental to a full dump, producing a new full dump file.

An incremental is a partial snapshot _with respect to_ the previous dump. The
Coda backup facility maintains an order on dumps for a volume.  **merge** will
only allow an incremental to be applied to its predecessor in the order.  This
predecessor may be a full dump or the output of the **merge**.

Once the administrator has created or retrieved the full dump which contains
the desired state of a volume, she can create a read-only copy of that state by
using the `volutil restore` facility.  This **volutil** command creates a new
read-only volume on a server.  The new volume can be mounted as any other Coda
volume.  Regular Unix file operations can then be used to extract the desired
old data.  The obvious exception is that mutating options will fail on files in
a read-only volume.

## Incremental Dumps

In every dump (full or incremental) produces a file containing the version
vectors and _StoreIds_ of every vnode in the volume.  These files have names of
the form `/vice/backup/<groupid>.<volid>.newlist`.  When the backup
coordinator is convinced that the backup of a volume has completed, the
`<filename>.newlist` file is renamed to be `<filename>.ancient` via the
`volutil ancient` call.  These files are stored in a human-readable format for
convenience.

When creating an incremental dump, the server looks for the .ancient file
corresponding to the volume.  If it doesnt exist, a full dump is created.  If
it does exist, it is used to determine which files have changed since the last
successful backup.  The server iterates through the vnode lists for the volume
and the version vector lists from the `<filename>.ancient` file comparing the
version vectors and storeIds.  A discrepancy between the two implies that the
file has changed and should be included in the incremental dump. By comparing
the sequence numbers in the vnodes lists, it can also be determined if a file
or directory had been deleted (since the vnode is no longer in use).  Vnodes
that are freed and then reallocated between dumps look like vnodes which have
been modified, and so are safely included in the incremental dump.

It is also important to maintain an ordering on the incremental dumps.  To
correctly restore to a particular day each incremental dump must be applied to
the appropriate full dump.  To ensure that this happens, each dump is labeled
with a uniquifier, and each incremental is labeled with the uniquifier of the
dump with respect to which it is taken.  During merge, the full dumps
uniquifier is compared with the uniquifier of the dump used to create the
incremental.  If they do not match, the incremental should not be applied to
the full dump.

## Tape Files

Once the dump files have been created, they must be written to tape. This is
due to the fact that disk space is usually a limited commodity.  The basic
mechanism for the writing is the Unix **tar.1** facility.

Each tape contains a series of tar files, the first and last of which are
labels. The start and end labels are identical, and contain version
information, the date the backup was taken, and an index which maps individual
dump files into offsets into the tape. Thus the Coda backup tapes are self
identifying for easy sanity checks. The label is a tar file which only contains
a simple Unix file called _TAPELABEL_.

The dump files are first sequenced by size.  They are then broken down into
groups, where the total size of the group must be larger than a certain size,
currently .5 Megabytes.  Each group is stored in a single tar file on the tape.
These data tar files are the 2nd through n-1st records on the tape, the first
and nth being a tar files containing just the tape label.

This structure was chosen for several reasons.  The first is that it is easy to
implement.  **tar** has been used for many years, and has been proven to be
reliable. The second is easy access of information on the tape.  Using a single
monolithic tar file would often require hours of waiting to retrieve a single
dump file. This way you can skip over most of the data using **mt.1** and its
fast-forward facility.  Finally, it provides a simple and effective end-to-end
check to validate that all the information has made it to tape.

At CMU, we have created a convention for capturing sufficient information for
reliability, while trying to avoid excess use of tapes.  Full backups are taken
once a week.  However, since our staging disks are not large enough to hold
full dumps for all the replicas of all the volumes, we stagger the full backups
across the week.

There are three kinds of requests for restorations: users who have mistakenly
trashed a file, users who lost data but didnt know it, or bugs which require us
to roll back to a substantially earlier state.  The first class of restores can
be typically handled by yesterdays state, which we keep on-line in the form of
read-only backup clones.  Thus almost all forms of requests never reach the
system administrator at all.  To give users easy access to the previous days
backup, create a directory, `OldFiles`, in their `coda` directory, and mount
each of the backups in the `OldFiles` directory.

If the user did not catch the loss of data immediately, its reasonable to
expect that they will catch it before a week has passed. We keep all
incremental and full backups to guarantee we can restore state from any day in
the last week.  This requires 14 tapes, or two weekly sets.  One weeks worth is
not sufficient, because state from later incremental backups relies on earlier
incremental backups in order to be restored.  Thus as soon as the first
incremental tape is over-written (say Mondays), the state from the remainder of
the last week is lost (last Tuesdays, Wednesdays, etc).

The third class of data loss is either due to infrequently used files or to
catastrophe.  (We've actually been forced to rely on the backup system to
restore _all_ Coda state, while developing server software). Since it is
unreasonable to keep all the tapes around, we only save tapes containing full
dumps.  Weekly tapes are saved for a month, and monthly tapes are saved for
eternity.

## Restoring a Backup Clone

A basic assumption of performing backups is that eventually someone will need
to restore old state of a volume.  To do this they should contact the system
administrator, specifying the volume and the date of the state they wish to
restore.

The system administrator must then determine which dump files contain the
state.  There could be more than one involved since the state may have been
captured by a full and some incremental dumps.  Once the administrator knows
the dates of the backups involved, she must get the appropriate tapes and
extract the dump files (via the **extract.sh** script).

The administrator then creates the full state to be restored by iteratively
applying the incremental backups to the full state via the **merge** program.
Once the state for the date in question has been restored, a read-only clone is
created by choosing a server to hold the clone, and invoking the `volutil
restore` operation, directing the call to the chosen server.  Once the clone
has been restored, the administrator should build a new VLDB, and mount the
volume in the Coda name space so the user can access it.  When the user has
finished with it, she should notify the administrator in order for the clone to
be purged.
(See also [how to restore a backup from tape](troubleshooting.md#server-problems))

## Backup Scripts

Although the **backup** program handles all the tricky details involved in Coda
backup, there still remain some issues to be handled, most notably the saving
of the dump files to tape.  This is done by a series of scripts, **backup.sh**,
and **tape.pl**.

**backup.sh** takes the name of the directory in which to run backups.  It
creates a subdirectory whose name indicates the date that backup was run.  It
then runs the **backup** program, using the dumplist file in the directory
specified in the arguments, saving the output of **backup** in a logfile in the
newly created subdirectory. It copies in the current Coda databases so they
will be saved to tape along with the dump files. It then invokes **tape.pl**
to write and verify that the files have been safely recorded.

**tape.pl** performs the work of saving the files on tape.  It takes the
directory in which the backup was taken (the subdirectory generated by
**backup.sh**), and the device name of the tape drive.  It first checks to see
that the tape to be used is either empty or has the correct label.  For Coda at
CMU this means checking that the tape was last used on the same day of the
week.  It then gathers the dump files and databases into groups and generates
the tape label for this backup.  Finally it writes the tape label and all the
groups to the tape via the **dump**(1) facility, marking the end of the tape
with another copy of the tape label.

**tape.pl** verifies that it did its job correctly.  It first reads off the
tape label, comparing it with one stored in the backup directory.  It then
scans all the dump files, listing the contents of the dumps and the command
line parameters for restore to stdout which has been redirected by
**backup.sh** to the backup logfile.

**restore**(1) is used to extract a dump file from a Coda backup tape.  The
backup logs contain the commandline that is needed to jump to the correct part
of the tape.
