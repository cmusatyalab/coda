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
"""Coda file system helpers.

These functions talk to the Coda client directly through the pioctl
transport (see the pioctl module) instead of wrapping the cfs
command-line tool.
"""

from __future__ import annotations

import errno
import os
import struct
from pathlib import Path
from typing import TYPE_CHECKING

from . import pioctl
from .pioctl import NotCodaFSError
from .structs import AclEntry, CodaFID, FidInfo, Tokens, VersionVector, VolumeStatus

if TYPE_CHECKING:
    from collections.abc import Iterable

# struct layouts, matching the C definitions (vcrcommon.rpc2, vice.rpc2,
# vproc_pioctl.cc)
_VICEFID_SIZE = 12  # 3 * uint32
_VICEVV_SIZE = 44  # 8 * int32 sites + uint32 store id + uint32 flags
_VOLUME_STATUS_SIZE = 40  # VolumeStatus
_REAM_LEN = 256  # MAXHOSTNAMELEN + 1
_SECRET_TOKEN_SIZE = 56  # sizeof(SecretToken)
_CLEAR_TOKEN_SIZE = 24  # sizeof(ClearToken)

# PRS_FS right bits and their characters, in canonical order (prs.h)
_RIGHTS = (
    ("r", 1),
    ("l", 8),
    ("i", 4),
    ("d", 16),
    ("w", 2),
    ("k", 32),
    ("a", 64),
)
_RIGHTS_BITS = dict(_RIGHTS)


def _check_error(code: int, command: str, path: Path | str | None) -> None:
    """Raise an exception corresponding to a pioctl error code"""
    if code == 0:
        return
    if code == errno.ENOENT:
        msg = f"{command} failed on {path}: no such file or directory"
        raise NotCodaFSError(msg)
    if code == errno.EACCES:
        msg = f"{command} failed on {path}: permission denied"
        raise PermissionError(msg)
    raise OSError(code, os.strerror(code), str(path))


def _rights_to_str(rights: int) -> str:
    """Return the canonical rights string for a PRS_FS bitmask"""
    return "".join(char for char, bit in _RIGHTS if rights & bit)


def _rights_to_int(rights: str) -> int:
    """Return the PRS_FS bitmask for a canonical rights string"""
    return sum(_RIGHTS_BITS[char] for char in rights)


def _cstring(data: bytes, offset: int) -> tuple[str, int]:
    """Read a null-terminated string, returning it and the next offset"""
    end = data.index(b"\x00", offset)
    return data[offset:end].decode(), end + 1


def _pack_volume_status(volstat: VolumeStatus) -> bytes:
    """Pack a VolumeStatus as the input of a SETVOLSTAT pioctl"""
    header = struct.pack(
        "<II4B7i",
        volstat.vid,
        volstat.parent_id,
        int(volstat.online),
        int(volstat.in_service),
        int(volstat.blessed),
        int(volstat.needs_salvage),
        volstat.type,
        volstat.min_quota,
        volstat.max_quota,
        volstat.blocks_in_use,
        volstat.part_blocks_avail,
        volstat.part_max_blocks,
        0,
    )
    return (
        header
        + volstat.name.encode()
        + b"\x00"
        + volstat.offlinemsg.encode()
        + b"\x00"
        + volstat.motd.encode()
        + b"\x00"
    )


def get_fid_info(path: Path | str) -> FidInfo:
    """Return Coda file identifier and version vector for specified path."""
    code, data = pioctl.pioctl(pioctl.VIOCGETFID, path)
    _check_error(code, "get_fid_info", path)

    volume, vnode, uniquifier = struct.unpack_from("<3I", data, 0)
    sites = struct.unpack_from("<8i", data, _VICEFID_SIZE)
    store_host, store_uniquifier, flags = struct.unpack_from(
        "<3I", data, _VICEFID_SIZE + 32
    )
    realm = data[_VICEFID_SIZE + _VICEVV_SIZE :].split(b"\x00", 1)[0].decode()

    return FidInfo(
        fid=CodaFID(volume, vnode, uniquifier, realm),
        vv=VersionVector(
            sites=list(sites),
            storeid_host=store_host,
            storeid_uniquifier=store_uniquifier,
            flags=flags,
        ),
    )


def get_fid(path: Path | str) -> CodaFID:
    """Return Coda file identifier for specified path."""
    return get_fid_info(path).fid


def get_vv(path: Path | str) -> VersionVector:
    """Return the version vector for the specified path."""
    return get_fid_info(path).vv


def get_parent_fid(fid: CodaFID) -> CodaFID:
    """Return the fid of the parent of the specified fid.

    If the fid is at a volume mount point, the fid of the mount point
    itself is returned.
    """
    in_data = struct.pack("<3I", fid.volume, fid.vnode, fid.uniquifier)
    in_data += fid.realm.encode() + b"\x00"
    code, data = pioctl.pioctl(pioctl.VIOCGETPFID, None, in_data=in_data)
    _check_error(code, "get_parent_fid", None)
    volume, vnode, uniquifier = struct.unpack_from("<3I", data, 0)
    return CodaFID(volume, vnode, uniquifier, fid.realm)


def get_acl(path: Path | str) -> list[AclEntry]:
    """Returns an ACL (list of AclEntry) for the specified path."""
    code, data = pioctl.pioctl(pioctl.VIOCGETAL, path, follow=True)
    _check_error(code, "get_acl", path)

    # The ACL is a text format: plus count, minus count, then the
    # plus entries followed by the minus entries, each on a line as
    # "<name><blank><rights>".
    lines = data.decode().split("\n")
    pluscount = int(lines[0])
    minuscount = int(lines[1])

    entries: list[AclEntry] = []
    for line in lines[2 : 2 + pluscount]:
        name, rights = line.split(None, 1)
        entries.append(AclEntry(name=name, rights=_rights_to_str(int(rights))))
    for line in lines[2 + pluscount : 2 + pluscount + minuscount]:
        name, rights = line.split(None, 1)
        entries.append(AclEntry(name=name, rights="-" + _rights_to_str(int(rights))))
    return entries


def set_acl(
    path: Path | str,
    acl: Iterable[tuple[str, str]],
    *,
    dry_run: bool = False,
) -> None:
    """Replaces ACL on specified path.

    The acl argument is an iterable of (name, rights) pairs.  Rights
    may use the shortcuts 'all' / 'none' and a leading '-' to denote
    a negative entry, and need not be in canonical order.
    """
    entries = [AclEntry.from_user(name, rights) for name, rights in acl]
    positives = [entry for entry in entries if entry.is_positive() and entry.rights]
    negatives = [entry for entry in entries if entry.is_negative() and entry.rights[1:]]

    if dry_run:
        listing = " ".join(
            f"{entry.name} {entry.rights}" for entry in positives + negatives
        )
        print(f"set_acl {path} {listing}".rstrip())
        return

    lines = [str(len(positives)), str(len(negatives))]
    for entry in positives:
        lines.append(f"{entry.name} {_rights_to_int(entry.rights)}")
    for entry in negatives:
        lines.append(f"-{entry.name} {_rights_to_int(entry.rights[1:])}")
    in_data = ("\n".join(lines) + "\n").encode() + b"\x00"

    code, _ = pioctl.pioctl(
        pioctl.VIOCSETAL, path, in_data=in_data, out_size=0, follow=True
    )
    _check_error(code, "set_acl", path)


def get_volume_status(path: Path | str) -> VolumeStatus:
    """Return the status of the volume containing the specified path."""
    code, data = pioctl.pioctl(pioctl.VIOCGETVOLSTAT, path, follow=True)
    _check_error(code, "get_volume_status", path)

    (
        vid,
        parent_id,
        online,
        in_service,
        blessed,
        needs_salvage,
        vtype,
        min_quota,
        max_quota,
        blocks_in_use,
        part_blocks_avail,
        part_max_blocks,
        _unused,
    ) = struct.unpack_from("<II4B7i", data, 0)

    offset = _VOLUME_STATUS_SIZE
    name, offset = _cstring(data, offset)
    conn_state, conflict, cml_count = struct.unpack_from("<3i", data, offset)
    offset += 12
    offlinemsg, offset = _cstring(data, offset)
    motd, offset = _cstring(data, offset)
    age, hogtime = struct.unpack_from("<2I", data, offset)
    offset += 8
    (cml_bytes,) = struct.unpack_from("<Q", data, offset)

    return VolumeStatus(
        vid=vid,
        parent_id=parent_id,
        online=online != 0,
        in_service=in_service != 0,
        blessed=blessed != 0,
        needs_salvage=needs_salvage != 0,
        type=vtype,
        min_quota=min_quota,
        max_quota=max_quota,
        blocks_in_use=blocks_in_use,
        part_blocks_avail=part_blocks_avail,
        part_max_blocks=part_max_blocks,
        name=name,
        conn_state=conn_state,
        conflict=conflict,
        cml_count=cml_count,
        offlinemsg=offlinemsg,
        motd=motd,
        age=age,
        hogtime=hogtime,
        cml_bytes=cml_bytes,
    )


def set_volume_status(
    path: Path | str,
    *,
    min_quota: int | None = None,
    max_quota: int | None = None,
    offlinemsg: str | None = None,
    motd: str | None = None,
    dry_run: bool = False,
) -> None:
    """Update the quota or messages of the volume at the specified path.

    Only the fields that are not None are changed, all others keep
    their current values.
    """
    if dry_run:
        changes = " ".join(
            f"{name}={value}"
            for name, value in (
                ("min_quota", min_quota),
                ("max_quota", max_quota),
                ("offlinemsg", offlinemsg),
                ("motd", motd),
            )
            if value is not None
        )
        print(f"set_volume_status {path} {changes}".rstrip())
        return

    volstat = get_volume_status(path)
    if min_quota is not None:
        volstat.min_quota = min_quota
    if max_quota is not None:
        volstat.max_quota = max_quota
    if offlinemsg is not None:
        volstat.offlinemsg = offlinemsg
    if motd is not None:
        volstat.motd = motd

    code, _ = pioctl.pioctl(
        pioctl.VIOCSETVOLSTAT, path, in_data=_pack_volume_status(volstat), follow=True
    )
    _check_error(code, "set_volume_status", path)


def get_tokens(realm: str) -> Tokens:
    """Return the authentication tokens for the calling user in a realm."""
    code, data = pioctl.pioctl(
        pioctl.VIOCGETTOK, None, in_data=realm.encode() + b"\x00"
    )
    _check_error(code, "get_tokens", None)

    (secret_size,) = struct.unpack_from("<I", data, 0)
    secret = data[4 : 4 + secret_size]
    (clear_size,) = struct.unpack_from("<I", data, 4 + secret_size)
    clear = data[8 + secret_size : 8 + secret_size + clear_size]
    return Tokens(secret=secret, clear=clear)


def set_tokens(
    realm: str,
    secret: bytes,
    clear: bytes,
    *,
    dry_run: bool = False,
) -> None:
    """Set the authentication tokens for the calling user in a realm."""
    if len(secret) != _SECRET_TOKEN_SIZE:
        msg = f"secret token must be {_SECRET_TOKEN_SIZE} bytes"
        raise ValueError(msg)
    if len(clear) != _CLEAR_TOKEN_SIZE:
        msg = f"clear token must be {_CLEAR_TOKEN_SIZE} bytes"
        raise ValueError(msg)

    if dry_run:
        print(f"set_tokens {realm}")
        return

    in_data = (
        struct.pack("<I", len(secret))
        + secret
        + struct.pack("<I", len(clear))
        + clear
        + realm.encode()
        + b"\x00"
    )
    code, _ = pioctl.pioctl(pioctl.VIOCSETTOK, None, in_data=in_data, out_size=0)
    _check_error(code, "set_tokens", None)


def invalidate_tokens(realm: str, *, dry_run: bool = False) -> None:
    """Invalidate the authentication tokens for the calling user."""
    if dry_run:
        print(f"invalidate_tokens {realm}")
        return

    code, _ = pioctl.pioctl(
        pioctl.VIOCUNLOG, None, in_data=realm.encode() + b"\x00", out_size=0
    )
    _check_error(code, "invalidate_tokens", None)


def set_venus_log(enabled: bool, *, dry_run: bool = False) -> None:
    """Enable or disable venus debug logging (root only)."""
    if dry_run:
        print(f"set_venus_log {enabled}")
        return

    in_data = struct.pack("<I", 1 if enabled else 0)
    code, _ = pioctl.pioctl(pioctl.VIOCVENUSLOG, None, in_data=in_data, out_size=0)
    _check_error(code, "set_venus_log", None)


def flush_cache(path: Path | str) -> None:
    """Invalidate the cache entry for the specified path."""
    code, _ = pioctl.pioctl(pioctl.VIOCFLUSH, path)
    _check_error(code, "flush_cache", path)


def prefetch(path: Path | str) -> None:
    """Prefetch the file at the specified path."""
    code, _ = pioctl.pioctl(pioctl.VIOCPREFETCH, path)
    _check_error(code, "prefetch", path)
