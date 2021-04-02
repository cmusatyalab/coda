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
""" Data structures used in the Python API """

import re
from collections import namedtuple

AclEntry = namedtuple("AclEntry", ["name", "rights"])
CodaFID = namedtuple("CodaFID", ["volume", "vnode", "uniquifier", "realm"])


def new_aclentry(name, rights):
    """ Coda ACL entry
        name   : a user or group name
        rights : a subset of '-rlidwka' (or 'all' / 'none' when setting)
    """

    # convert
    if rights is None or rights == "none":
        rights = ""
    elif rights in ["all", "*"]:
        rights = "rlidwka"

    # validate
    match = re.match("^-?r?l?i?d?w?k?a?$", rights)
    if match is None:
        raise ValueError(
            "rights must be a subset of " "'-rlidwka', 'all', '*', or 'none'"
        )

    return AclEntry(name, rights)
