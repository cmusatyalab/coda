#
#                          Coda File System
#                             Release 8
#
#             Copyright (c) 2021-2026 Carnegie Mellon University
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
from dataclasses import dataclass


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
    rights: str

    def __post_init__(self) -> None:
        _aclentry_validate_rights(self, None, self.rights)

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


@dataclass
class VersionVector:
    """Coda version vector
    sites : per-site version counters (8 entries)
    storeid_host : host id of the site that last updated the object
    storeid_uniquifier : uniquifier of the last update
    flags : version vector flags
    """

    sites: list[int]
    storeid_host: int
    storeid_uniquifier: int
    flags: int


@dataclass
class FidInfo:
    """Coda file identifier and version vector for a path"""

    fid: CodaFID
    vv: VersionVector


@dataclass
class VolumeStatus:
    """Status of a Coda volume
    vid : volume identifier
    parent_id : id of the parent volume, if this is a replicated volume
    online : volume is online
    in_service : volume is in service (writable)
    blessed : volume is blessed (read-write)
    needs_salvage : volume needs salvage
    type : volume type
    min_quota : minimum volume quota
    max_quota : maximum volume quota
    blocks_in_use : blocks in use
    part_blocks_avail : blocks available on the partition
    part_max_blocks : maximum blocks on the partition
    name : volume name
    conn_state : connection state of the volume's servers
    conflict : number of conflicts in the volume
    cml_count : number of cached entries for the volume
    offlinemsg : message displayed when the volume is offline
    motd : message of the day for the volume
    age : seconds since the volume was last integrated
    hogtime : seconds since the volume last hogged a server
    cml_bytes : bytes cached for the volume
    """

    vid: int
    parent_id: int
    online: bool
    in_service: bool
    blessed: bool
    needs_salvage: bool
    type: int
    min_quota: int
    max_quota: int
    blocks_in_use: int
    part_blocks_avail: int
    part_max_blocks: int
    name: str
    conn_state: int
    conflict: int
    cml_count: int
    offlinemsg: str
    motd: str
    age: int
    hogtime: int
    cml_bytes: int


@dataclass
class Tokens:
    """Authentication tokens for a Coda realm
    secret : opaque secret token
    clear : clear text token
    """

    secret: bytes
    clear: bytes
