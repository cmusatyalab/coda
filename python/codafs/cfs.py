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
    """ Exception 'trying to do Coda operation on non-Coda file system' """


_commands = {}


# test if required binaries exist
def _which(name, hint):
    if name not in _commands:
        path = find_executable(name)
        if path is None:
            raise FileNotFoundError("Cannot find {}, {}".format(name, hint))
        _commands[name] = path
    return _commands[name]


def getfid(path):
    """ return Coda file identifier for specified path """
    cfs = _which("cfs", "check your Coda client installation")
    try:
        result = subprocess.check_output([cfs, "getfid", str(path)])
    except subprocess.CalledProcessError:
        raise NotCodaFS("cfs getfid failed on {}".format(path))

    match = re.match(
        r"^FID = ([0-9a-fA-F]+)\.([0-9a-fA-F]+)\.([0-9a-fA-F]+)@(\S+)",
        result.decode("ascii"),
    )
    if match is None:
        raise NotCodaFS("cfs getfid unexpected output on {}".format(path))
    return CodaFID(*match.groups())


def listvol(path):
    """ return volume_id, name tuple for specified path """
    cfs = _which("cfs", "check your Coda client installation")
    try:
        result = subprocess.check_output(
            [cfs, "listvol", str(path)], stderr=subprocess.STDOUT
        )
    except subprocess.CalledProcessError:
        raise NotCodaFS("cfs listvol failed on {}".format(path))

    result = result.decode("ascii")
    if result.endswith(": Permission denied\n"):
        raise PermissionError("cfs listvol permission denied on {}".format(path))

    match = re.search(r' volume ([0-9a-fA-F]+) .* named "([^"]+)"', result)
    if match is None:
        raise NotCodaFS("cfs listvol unexpected output on {}".format(path))
    return match.groups()


def listacl(path):
    """ returns an ACL (list of user/group, rights tuples) for the specified path """
    cfs = _which("cfs", "check your Coda client installation")
    try:
        result = subprocess.check_output([cfs, "listacl", str(path)])
    except subprocess.CalledProcessError:
        raise NotCodaFS("cfs listvol failed on {}".format(path))

    return [
        AclEntry(*entry)
        for entry in re.findall(
            r"\s*(\S+)\s+(-?r?l?i?d?w?k?a?)", result.decode("ascii")
        )
    ]


def setacl(path, acl):
    """ Replaces ACL on specified path """
    cfs = _which("cfs", "check your Coda client installation")
    _acl = [AclEntry.from_user(*entry) for entry in acl]

    positives = [(entry.name, entry.rights) for entry in _acl if entry.is_positive()]
    negatives = [
        (entry.name, entry.rights[1:]) for entry in _acl if entry.is_negative()
    ]

    subprocess.call(list(itertools.chain([cfs, "sa", "-clear", str(path)], *positives)))
    if negatives:
        subprocess.call(
            list(itertools.chain([cfs, "sa", "-negative", str(path)], *negatives))
        )
