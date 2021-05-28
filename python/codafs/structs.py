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
""" Data structures used in the Python API """

# pylint: disable=too-few-public-methods

import re

import attr


def _aclentry_validate_rights(_instance, _attribute, value):
    """rights should be a subset of '-rlidwka'"""
    if not isinstance(value, str) or re.match(r"^-?r?l?i?d?w?k?a?$", value) is None:
        raise ValueError("AclEntry.rights has to be a subset of '-rlidwka'")


@attr.s
class AclEntry:
    """Coda ACL entry
    name   : a user or group name
    rights : a subset of '-rlidwka' (or 'all' / 'none' when setting)
    """

    name = attr.ib(validator=attr.validators.instance_of(str))
    rights = attr.ib(validator=_aclentry_validate_rights)

    @classmethod
    def from_user(cls, name, rights):
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

    def is_positive(self):
        """returns true if this is a positive ACL entry"""
        return not self.rights.startswith("-")

    def is_negative(self):
        """returns true if this is a negative ACL entry"""
        return self.rights.startswith("-")


@attr.s
class CodaFID:
    """Coda File identifier
    volume : volume identifier
    vnode :  object identifier
    uniquifier : unique object identifier
    realm : Coda administrative domain
    """

    volume = attr.ib()
    vnode = attr.ib()
    uniquifier = attr.ib()
    realm = attr.ib()
