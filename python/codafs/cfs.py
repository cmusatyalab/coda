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

from .structs import new_aclentry

_commands = {}


# test if required binaries exist
def _which(name, hint):
    if name not in _commands:
        path = find_executable(name)
        if path is None:
            raise FileNotFoundError("Cannot find {}, {}".format(name, hint))
        _commands[name] = path
    return _commands[name]


def listvol(path):
    """ return volume_id, name tuple for specified path """
    cfs = _which("cfs", "check your Coda client installation")
    result = subprocess.run([cfs, "listvol", path], capture_output=True, check=False)
    match = re.search(
        r' volume ([0-9a-fA-F]+) .* named "([^"]+)"', result.stdout.decode("ascii")
    )
    if match is None:
        return None
    return match.group(0), match.group(1)


def listacl(path):
    """ returns an ACL (list of user/group, rights tuples) for the specified path """
    cfs = _which("cfs", "check your Coda client installation")
    result = subprocess.run([cfs, "listacl", path], capture_output=True, check=True)
    return [
        new_aclentry(*entry)
        for entry in re.findall(
            r"\s*(\S+)\s+(-?r?l?i?d?w?k?a?)", result.stdout.decode("ascii")
        )
    ]


def setacl(path, acl):
    """ Replaces ACL on specified path """
    cfs = _which("cfs", "check your Coda client installation")
    _acl = [new_aclentry(*entry) for entry in acl]

    positives = [
        (entry.name, entry.rights) for entry in _acl if not entry.rights.startswith("-")
    ]
    negatives = [
        (entry.name, entry.rights[1:]) for entry in _acl if entry.rights.startswith("-")
    ]

    subprocess.run(
        list(itertools.chain([cfs, "sa", "-clear", path], *positives)), check=True
    )
    if negatives:
        subprocess.run(
            list(itertools.chain([cfs, "sa", "-negative", path], *negatives)),
            check=True,
        )
