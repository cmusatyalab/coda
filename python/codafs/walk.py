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
"""Helper function to walk a volume in the Coda file system."""

from __future__ import annotations

import logging
from typing import TYPE_CHECKING, Callable

if TYPE_CHECKING:
    from collections.abc import Iterator
    from pathlib import Path

from .cfs import NotCodaFSError, getfid, listvol


def default_volume_callback(_root: Path, _volume_name: str, _volume_id: str) -> None:
    """Default volume callback, avoids crossing volume boundaries."""
    raise StopIteration


def walk_volumes(
    roots: list[Path],
    volume_callback: Callable[[Path, str, str], None] | None = default_volume_callback,
) -> Iterator[Path]:
    for root in roots:
        yield from walk_volume(root, volume_callback=volume_callback)


def walk_volume(
    root: Path,
    volume_callback: Callable[[Path, str, str], None] | None = default_volume_callback,
    parent_volume_id: str | None = None,
) -> Iterator[Path]:
    """Path walking, but with Coda volume awareness.

    The volume_callback will be called whenever a volume mountpoint is found,
    it may raise a StopIteration exception to avoid cross-volume crawling.
    """
    try:
        volume_id = getfid(root).volume
    except NotCodaFSError:
        logging.critical("%s is not a path in Coda", root)
        return

    if parent_volume_id is None:
        parent_volume_id = volume_id

    elif volume_id != parent_volume_id and volume_callback is not None:
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
                path,
                volume_callback=volume_callback,
                parent_volume_id=volume_id,
            )
        except PermissionError:
            logging.warning("Unable to iterate over %s", path)
