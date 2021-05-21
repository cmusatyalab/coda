# -*- coding: utf-8 -*-
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
""" Wrappers around various cfs commands """

import itertools
import re
import subprocess
from distutils.spawn import find_executable

from .structs import AclEntry, CodaFID


class NotCodaFS(FileNotFoundError):
    """Exception 'trying to do Coda operation on non-Coda file system'"""


# Cached path to the cfs executable
_CFS_COMMAND = None


def cfs(*args):
    """locate cfs command and run it with the given arguments"""
    global _CFS_COMMAND  # pylint: disable=global-statement
    if _CFS_COMMAND is None:
        _CFS_COMMAND = find_executable("cfs")
        if _CFS_COMMAND is None:
            raise FileNotFoundError(
                "Cannot find cfs, check your Coda client installation"
            )

    # try to run cfs command
    try:
        result = subprocess.check_output(
            (_CFS_COMMAND,) + args, stderr=subprocess.STDOUT
        )
    except subprocess.CalledProcessError as exc:
        raise NotCodaFS("cfs {} failed".format(args[0])) from exc

    return result.decode("ascii")


def getfid(path):
    """return Coda file identifier for specified path"""
    result = cfs("getfid", str(path))

    match = re.match(
        r"^FID = ([0-9a-fA-F]+)\.([0-9a-fA-F]+)\.([0-9a-fA-F]+)@(\S+)", result
    )
    if match is None:
        raise NotCodaFS("cfs getfid unexpected output on {}".format(path))
    return CodaFID(*match.groups())


def listvol(path):
    """return volume_id, name tuple for specified path"""
    result = cfs("listvol", str(path))

    if result.endswith(": Permission denied\n"):
        raise PermissionError("cfs listvol permission denied on {}".format(path))

    match = re.search(r' volume ([0-9a-fA-F]+) .* named "([^"]+)"', result)
    if match is None:
        raise NotCodaFS("cfs listvol unexpected output on {}".format(path))
    return match.groups()


def listacl(path):
    """returns an ACL (list of user/group, rights tuples) for the specified path"""
    result = cfs("listacl", str(path))

    return [
        AclEntry(name, rights)
        for name, rights in re.findall(r"\s*(\S+)\s+(-?r?l?i?d?w?k?a?)", result)
    ]


def setacl(path, acl):
    """Replaces ACL on specified path"""
    _acl = [AclEntry.from_user(name, rights) for name, rights in acl]

    positives = [(entry.name, entry.rights) for entry in _acl if entry.is_positive()]
    negatives = [
        (entry.name, entry.rights[1:]) for entry in _acl if entry.is_negative()
    ]

    cfs("sa", "-clear", str(path), *itertools.chain(*positives))
    if negatives:
        cfs("sa", "-negative", str(path), *itertools.chain(*negatives))
