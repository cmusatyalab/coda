#
#                          Coda File System
#                             Release 8
#
#             Copyright (c) 2021 Carnegie Mellon University
#
# This  code  is  distributed "AS IS" without warranty of any kind under
# the terms of the GNU General Public License Version 2, as shown in the
# file  LICENSE.  The  technical and financial  contributors to Coda are
# listed in the file CREDITS.
#
"""Wrappers around various cfs commands."""

import itertools
import re
from pathlib import Path

from .structs import AclEntry, CodaFID
from .util import ExecutionError, check_output


class NotCodaFSError(ExecutionError):
    """Exception 'trying to do Coda operation on non-Coda file system'."""


def getfid(path: Path) -> CodaFID:
    """Return Coda file identifier for specified path."""
    result = check_output("cfs", "getfid", str(path))

    match = re.match(
        r"^FID = ([0-9a-fA-F]+)\.([0-9a-fA-F]+)\.([0-9a-fA-F]+)@(\S+)",
        result,
    )
    if match is None:
        msg = f"cfs getfid unexpected output on {path}"
        raise NotCodaFSError(msg)
    return CodaFID(*match.groups())


def listvol(path: Path) -> list[tuple[str, str]]:
    """Return volume_id, name tuple for specified path."""
    result = check_output("cfs", "listvol", str(path))

    if result.endswith(": Permission denied\n"):
        msg = f"cfs listvol permission denied on {path}"
        raise PermissionError(msg)

    match = re.search(r' volume ([0-9a-fA-F]+) .* named "([^"]+)"', result)
    if match is None:
        msg = f"cfs listvol unexpected output on {path}"
        raise NotCodaFSError(msg)
    return match.groups()


def listacl(path: Path) -> list[AclEntry]:
    """Returns an ACL (list of user/group, rights tuples) for the specified path."""
    result = check_output("cfs", "listacl", str(path))

    return [
        AclEntry(name, rights)
        for name, rights in re.findall(r"\s*(\S+)\s+(-?r?l?i?d?w?k?a?)", result)
    ]


def setacl(path: Path, acl: AclEntry, *, dry_run: bool = False) -> None:
    """Replaces ACL on specified path."""
    _acl = [AclEntry.from_user(name, rights) for name, rights in acl]

    positives = [(entry.name, entry.rights) for entry in _acl if entry.is_positive()]
    negatives = [
        (entry.name, entry.rights[1:]) for entry in _acl if entry.is_negative()
    ]

    check_output(
        "cfs",
        "sa",
        "-clear",
        str(path),
        *itertools.chain(*positives),
        dry_run=dry_run,
    )
    if negatives:
        check_output(
            "cfs",
            "sa",
            "-negative",
            str(path),
            *itertools.chain(*negatives),
            dry_run=dry_run,
        )
