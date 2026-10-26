#
#                          Coda File System
#                             Release 8
#
#             Copyright (c) 2026 Carnegie Mellon University
#
# This  code  is  distributed "AS IS" without warranty of any kind under
# the terms of the GNU General Public License Version 2, as shown in the
# file  LICENSE.  The  technical and financial  contributors to Coda are
# listed in the file CREDITS.
#
"""Direct pioctl access to the Coda client.

A Coda client (venus) exposes a virtual file named
"...PIOCTL.<unique>" at the root of the Coda mount point.  A pioctl is
issued by writing a request header, a path and a data buffer to the
file, closing the file, and reading the response, which the client
truncates and rewrites into the same file.  The request format is::

    <cmd>\\n<plen>\\n<follow>\\n<in_size>\\n<out_size>\\0<path><in data>

and the response format is::

    <code>\\n<size>\\0<out data>

where <code> is 0 or an errno value.

The command numbers mirror venusioctl.h.
"""

from __future__ import annotations

import errno
import os
import secrets
import threading
import time
from pathlib import Path

from .util import ExecutionError

# pioctl limits (coda.h, venusioctl.h)
CFS_PIOBUFSIZE = 2048  # maximum size of a pioctl in/out buffer
CODA_MAXPATHLEN = 1024  # maximum length of a pioctl path

# how long to wait for the client to process a pioctl
PIOCTL_TIMEOUT = 60.0

# how many pioctl files may be in flight at once, per process.  The client
# services each pioctl on a worker thread drawn from its message worker
# pool (20 by default), which is shared with all other VFS traffic on the
# workstation, and the thread is busy for as long as the command takes, so
# keep the fan-out well below the pool size.
MAX_INFLIGHT_PIOCTLS = 8
_inflight_pioctls = threading.BoundedSemaphore(MAX_INFLIGHT_PIOCTLS)

# prefix of the pioctl virtual file (pioctl.h)
PIOCTL_PREFIX = "...PIOCTL."

# base for CFS-specific pioctl commands (venusioctl.h)
CFS_IOCTL_BASE = 192

# pioctl command numbers (venusioctl.h)
VIOCSETAL = 1  # Set access control list
VIOCGETAL = 2  # Get access control list
VIOCSETTOK = 3  # Set authentication tokens
VIOCGETVOLSTAT = 4  # Get volume status
VIOCSETVOLSTAT = 5  # Set volume status
VIOCFLUSH = 6  # Invalidate cache entry
VIOCSTAT = 7  # Get file status
VIOCGETTOK = 8  # Get authentication tokens
VIOCUNLOG = 9  # Invalidate tokens
VIOCCKSERV = 10  # Check that servers are up
VIOCCKBACK = 11  # Check backup volume mappings
VIOCCKCONN = 12  # Check connections for a user
VIOCWHEREIS = 14  # Find out where a volume is located
VIOCPREFETCH = 15  # Prefetch a file
VIOCNOP = 16  # Do nothing (more performance)
VIOCENGROUP = 17  # Enable group access for a group
VIOCDISGROUP = 18  # Disable group access
VIOCLISTGROUPS = 19  # List enabled and disabled groups
VIOCACCESS = 20  # Access using PRS_FS bits
VIOCUNPAG = 21  # Invalidate pag
VIOCGETWD = 22  # Get wdir quickly
VIOCWAITFOREVER = 23  # Wait for dead servers forever
VIOCSETCACHESIZE = 24  # Set venus cache size in 1k units
VIOCFLUSHCB = 25  # Flush callback only
VIOCNEWCELL = 26  # Configure new cell
VIOCGETCELL = 27  # Get cell info
VIOC_AFS_DELETE_MT_PT = 28  # [AFS] Delete mount point
VIOC_AFS_STAT_MT_PT = 29  # [AFS] Stat mount point
VIOC_FILE_CELL_NAME = 30  # Get cell in which file lives
VIOC_GET_WS_CELL = 31  # Get cell in which workstation lives
VIOC_AFS_MARINER_HOST = 32  # [AFS] Get/set mariner host
VIOC_GET_PRIMARY_CELL = 33  # Get primary cell for caller
VIOCVENUSLOG = 34  # Enable/Disable venus logging
VIOC_GETCELLSTATUS = 35  # Get cell status info
VIOC_SETCELLSTATUS = 36  # Set corresponding cell status info
VIOC_FLUSHVOLUME = 37  # Flush whole volume's data
VIOC_LISTCACHE_VOLUME = 39  # List volume's cached status
VIOC_LOOKASIDE = 40  # Move an fso to the lookaside cache
VIOC_EXPANDOBJECT = 41  # Expand a compressed fso
VIOC_COLLAPSEOBJECT = 42  # Collapse an expanded fso
VIOC_SYNCCACHE = 140  # Reintegrate now
VIOC_REDIR = 141  # Redirect to a staging server
VIOC_ADD_MT_PT = 142  # Add mount point
VIOC_ENABLEREPAIR = CFS_IOCTL_BASE  # Enable local/global repair
VIOC_DISABLEREPAIR = CFS_IOCTL_BASE + 1  # Disable local/global repair
VIOC_REPAIR = CFS_IOCTL_BASE + 2  # Repair an object
VIOC_GETSERVERSTATS = CFS_IOCTL_BASE + 3  # Get host statistics
VIOC_GETVENUSSTATS = CFS_IOCTL_BASE + 4  # Get Venus statistics
VIOCGETFID = CFS_IOCTL_BASE + 5  # Get ViceFid
VIOCFLUSHCACHE = CFS_IOCTL_BASE + 6  # Flush entire FSO cache
VIOCSETVV = CFS_IOCTL_BASE + 7  # Overwrite a version vector
VIOC_HDB_ADD = CFS_IOCTL_BASE + 8  # Add hoard entry
VIOC_HDB_DELETE = CFS_IOCTL_BASE + 9  # Delete hoard entry
VIOC_HDB_MODIFY = CFS_IOCTL_BASE + 10  # Modify hoard entry
VIOC_HDB_CLEAR = CFS_IOCTL_BASE + 11  # Clear hoard database
VIOC_HDB_LIST = CFS_IOCTL_BASE + 12  # List hoard database
VIOC_WAITFOREVER = CFS_IOCTL_BASE + 13  # Control waitforever behavior
VIOC_HDB_WALK = CFS_IOCTL_BASE + 14  # Walk hoard database
VIOC_CLEARPRIORITIES = CFS_IOCTL_BASE + 15  # Clear short-term priorities
VIOCGETPATH = CFS_IOCTL_BASE + 16  # Map Fid to vol-relative path
VIOC_CHECKPOINTML = CFS_IOCTL_BASE + 19  # Checkpoint a volume's ML
VIOC_PURGEML = CFS_IOCTL_BASE + 20  # Purge a volume's ML
VIOC_BEGINRECORDING = CFS_IOCTL_BASE + 21  # Begin recording references
VIOC_ENDRECORDING = CFS_IOCTL_BASE + 22  # End recording references
VIOC_TRUNCATELOG = CFS_IOCTL_BASE + 23  # Truncate the log
VIOC_DISCONNECT = CFS_IOCTL_BASE + 24  # Partition from all servers
VIOC_RECONNECT = CFS_IOCTL_BASE + 25  # Heal previous partition
VIOCGETPFID = CFS_IOCTL_BASE + 27  # Get fid of object's parent
VIOC_WD = CFS_IOCTL_BASE + 28  # Set write-disconnect parameters
VIOC_HDB_VERIFY = CFS_IOCTL_BASE + 30  # Compare hoard db to cache contents
VIOC_HDB_ENABLE = CFS_IOCTL_BASE + 32  # Enable hoard walks
VIOC_HDB_DISABLE = CFS_IOCTL_BASE + 33  # Disable hoard walks
VIOC_ENABLEASR = CFS_IOCTL_BASE + 34  # Enable ASRs for a volume
VIOC_DISABLEASR = CFS_IOCTL_BASE + 35  # Disable ASRs for a volume
VIOC_FLUSHASR = CFS_IOCTL_BASE + 36  # Flush timestamp of last ASR call
VIOC_REP_BEGIN = CFS_IOCTL_BASE + 37  # Begin a repair session
VIOC_REP_END = CFS_IOCTL_BASE + 38  # End a repair session
VIOC_REP_CHECK = CFS_IOCTL_BASE + 39  # Check the current local mutation
VIOC_REP_PRESERVE = CFS_IOCTL_BASE + 40  # Preserve current local mutation
VIOC_REP_DISCARD = CFS_IOCTL_BASE + 41  # Discard current local mutation
VIOC_REP_REMOVE = CFS_IOCTL_BASE + 42  # Remove rest of local mutations
VIOC_SET_LOCAL_VIEW = CFS_IOCTL_BASE + 43  # Set local repair session view
VIOC_SET_GLOBAL_VIEW = CFS_IOCTL_BASE + 44  # Set global repair session view
VIOC_SET_MIXED_VIEW = CFS_IOCTL_BASE + 45  # Set mixed repair session view
VIOC_WD_ALL = CFS_IOCTL_BASE + 46  # Set write-disconnect on all volumes
VIOC_SYNCCACHE_ALL = CFS_IOCTL_BASE + 47  # Reintegrate all volumes
VIOC_LISTCACHE = CFS_IOCTL_BASE + 50  # List cache status
VIOC_GET_MT_PT = CFS_IOCTL_BASE + 51  # Get mount point path from vid
VIOC_REP_CMD = CFS_IOCTL_BASE + 52  # Local/global repair command
VIOC_UNLOADKERNEL = CFS_IOCTL_BASE + 53  # Unload kernel module
VIOC_ZLIMIT = CFS_IOCTL_BASE + 54  # Set red & yellow zone limits


class NotCodaFSError(ExecutionError):
    """Exception 'trying to do Coda operation on non-Coda file system'."""


# configuration directories searched for venus.conf when CODACONFPATH is
# not set; mirrors codaconf.c (SYSCONFDIR:/usr/local/etc/coda:/etc/coda,
# with SYSCONFDIR depending on the install prefix)
_DEFAULT_CONF_DIRS = "/usr/coda/etc:/usr/local/etc:/usr/local/etc/coda:/etc/coda"


def get_mountpoint() -> str:
    """Return the Coda mount point.

    Reads the 'mountpoint' key from venus.conf, searching the
    directories named in the CODACONFPATH environment variable, or
    a standard set of configuration directories if that is not set.
    Returns '/coda' if the key is not found.
    """
    confpath = os.environ.get("CODACONFPATH", _DEFAULT_CONF_DIRS)
    for directory in confpath.split(":"):
        if not directory:
            continue
        try:
            text = (Path(directory) / "venus.conf").read_text()
        except OSError:
            continue
        for line in text.splitlines():
            parts = line.split()
            if len(parts) >= 2 and parts[0] == "mountpoint":
                return parts[1]
    return "/coda"


def _strip_mount_prefix(path: Path | str | None, mountpoint: str) -> str:
    """Return path with the mountpoint prefix stripped.

    The pioctl file carries paths relative to the Coda mount point;
    a None path means a filesystem-wide command.
    """
    if path is None:
        return "/"
    relpath = os.path.abspath(os.path.expanduser(str(path)))
    root = mountpoint.rstrip("/")
    if root and relpath == root:
        return ""
    prefix = root + "/"
    if relpath.startswith(prefix):
        return relpath[len(root) :]
    raise NotCodaFSError(
        f"{path} is not a path in the Coda file system (mounted at {mountpoint})"
    )


def pioctl(
    cmd: int,
    path: Path | str | None,
    *,
    follow: bool = False,
    in_data: bytes = b"",
    out_size: int = CFS_PIOBUFSIZE,
    timeout: float = PIOCTL_TIMEOUT,
) -> tuple[int, bytes]:
    """Issue a pioctl against the Coda client.

    Args:
        cmd: pioctl command number (e.g. VIOCGETFID)
        path: path within the Coda file system, or None for
            filesystem-wide commands
        follow: follow symbolic links when resolving path
        in_data: request data
        out_size: maximum expected response size
        timeout: seconds to wait for the client to process the pioctl

    Returns:
        (code, data), where code is 0 on success or the errno value
        returned by the Coda client, and data is the response data.

    Raises:
        NotCodaFSError: if path is not within the Coda file system
        ValueError: if cmd, in_data or out_size are out of range
    """
    if not 0 <= cmd <= 255:
        msg = f"pioctl command {cmd} out of range"
        raise ValueError(msg)
    if len(in_data) > CFS_PIOBUFSIZE:
        msg = f"pioctl request data is {len(in_data)} bytes"
        raise ValueError(msg)
    if not 0 <= out_size <= CFS_PIOBUFSIZE:
        msg = f"pioctl response size {out_size} out of range"
        raise ValueError(msg)

    mountpoint = get_mountpoint()
    relpath = _strip_mount_prefix(path, mountpoint)
    if len(relpath) > CODA_MAXPATHLEN:
        msg = f"{path} is too long to be a path in Coda"
        raise NotCodaFSError(msg)

    # The kernel never completes a pioctl whose target path does not
    # exist (the pioctl file is left in a stuck EIO state), so detect the
    # case up front with a plain stat, which returns ENOENT immediately
    # without going through the client.  A non-following pioctl can
    # legitimately target a dangling symbolic link, so in that case the
    # check must not follow links either.
    if path is not None:
        try:
            if follow:
                os.stat(path)
            else:
                os.lstat(path)
        except (FileNotFoundError, NotADirectoryError):
            msg = f"{path} is not a path in the Coda file system"
            raise NotCodaFSError(msg) from None

    unique = secrets.randbits(32)
    pioctl_file = Path(mountpoint) / f"{PIOCTL_PREFIX}{unique:08x}"

    header = (
        f"{cmd}\n{len(relpath)}\n{int(follow)}\n{len(in_data)}\n{out_size}\x00"
    ).encode()
    with _inflight_pioctls:
        with open(pioctl_file, "wb") as pfile:
            pfile.write(header + relpath.encode() + in_data)
        # the client processes the pioctl when the file is closed and
        # rewrites the file with the response

        # The client processes the pioctl asynchronously: until it rewrites
        # the file with the response, reads return an empty file or EIO.
        deadline = time.monotonic() + timeout
        while True:
            try:
                with open(pioctl_file, "rb") as pfile:
                    raw = pfile.read()
            except OSError as e:
                if e.errno == errno.EIO:
                    raw = b""  # still being processed
                elif e.errno == errno.ENOENT:
                    msg = f"no response from Coda client in {pioctl_file}"
                    raise OSError(errno.EBADF, msg) from None
                else:
                    raise
            parts = raw.split(b"\n", 2)
            if (
                len(parts) == 3
                and parts[0].isdigit()
                and parts[1].isdigit()
                and parts[2].startswith(b"\x00")
            ):
                code = int(parts[0])
                size = int(parts[1])
                if size > out_size:
                    msg = f"pioctl response too large in {pioctl_file}"
                    raise OSError(errno.EBADF, msg)
                data = parts[2][1 : 1 + size]
                if len(data) == size:
                    return code, data
            if time.monotonic() >= deadline:
                msg = f"timed out waiting for Coda client response in {pioctl_file}"
                raise TimeoutError(msg)
            time.sleep(0.05)
