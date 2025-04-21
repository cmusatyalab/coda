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
"""Data structures used in the Python API"""

# pylint: disable=too-few-public-methods

from __future__ import annotations

import re
from dataclasses import InitVar, dataclass


def _aclentry_validate_rights(_instance: object, _attribute: str, value: str) -> None:
    """Rights should be a subset of '-rlidwka'"""
    if not isinstance(value, str) or re.match(r"^-?r?l?i?d?w?k?a?$", value) is None:
        raise ValueError("AclEntry.rights has to be a subset of '-rlidwka'")


@dataclass
class AclEntry:
    """Coda ACL entry
    name   : a user or group name
    rights : a subset of '-rlidwka' (or 'all' / 'none' when setting)
    """

    name: str
    rights: InitVar[str]

    def __post_init__(self, rights: str) -> None:
        _aclentry_validate_rights(self, None, rights)

    @classmethod
    def from_user(cls, name, rights) -> AclEntry:
        """Create a new AclEntry instance from user input.
        Shortcuts like '*', 'all', and 'none' are
        acceptable as well as a different ordering.
        """
        canon_rights = ""
        if rights in ["all", "*"]:
            canon_rights = "rlidwka"

        elif rights is not None and rights not in ["none"]:
            for permission in "-rlidwka":
                if permission in rights:
                    canon_rights += permission

        return cls(name=name, rights=canon_rights)

    def is_positive(self) -> bool:
        """Returns true if this is a positive ACL entry"""
        return not self.is_negative()

    def is_negative(self) -> bool:
        """Returns true if this is a negative ACL entry"""
        return self.rights.startswith("-")


@dataclass
class CodaFID:
    """Coda File identifier
    volume : volume identifier
    vnode :  object identifier
    uniquifier : unique object identifier
    realm : Coda administrative domain
    """

    volume: int
    vnode: int
    uniquifier: int
    realm: str
