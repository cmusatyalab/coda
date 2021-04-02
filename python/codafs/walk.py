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
""" Helper function to walk a volume in the Coda file system """

import logging

from .cfs import NotCodaFS, getfid, listvol


def default_volume_callback(_root, _volume_name, _volume_id):
    """ Example volume callback, this just avoids crossing volume boundaries """
    raise StopIteration


def walk_volume(root, volume_callback=default_volume_callback, parent_volume_id=None):
    """ Path walking, but with Coda volume awareness

    The volume_callback will be called whenever a volume mountpoint is found,
    it may raise a StopIteration exception to avoid cross-volume crawling.
    """
    try:
        volume_id, _, _, _ = getfid(root)
    except NotCodaFS:
        logging.critical("%s is not a path in Coda", root)
        return

    if parent_volume_id is None:
        parent_volume_id = volume_id

    elif volume_id != parent_volume_id:
        if volume_callback is not None:
            try:
                _, volume_name = listvol(root)
                volume_callback(root, volume_name, volume_id)
            except StopIteration:
                return

    yield root

    for path in root.iterdir():
        if not path.is_dir():
            continue
        try:
            yield from walk_volume(
                path, volume_callback=volume_callback, parent_volume_id=volume_id
            )
        except PermissionError:
            logging.warning("Unable to iterate over %s", path)
