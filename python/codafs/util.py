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
"""Various helper commands"""

import subprocess
from distutils.spawn import find_executable
from subprocess import DEVNULL
from typing import Any

__all__ = ["DEVNULL", "ExecutionError", "check_output", "run"]


class ExecutionError(Exception):
    """Command execution returned and error"""


# cache command path lookups
_COMMAND_CACHE: dict[str, str] = {}


def _cached(command: str, *args: str) -> list[str]:
    """Locate command and return it path"""
    try:
        return (_COMMAND_CACHE[command],) + args
    except KeyError:
        pass

    filepath = find_executable(command)
    if filepath is None:
        msg = f"Cannot find {command}"
        raise FileNotFoundError(msg)
    return (_COMMAND_CACHE.setdefault(command, filepath),) + args


def run(*args: str, **kwargs: dict[str, Any]) -> None:
    """Locate command and run it with the given arguments"""
    if kwargs.pop("dry_run", False):
        print(" ".join(str(arg) for arg in args))
        return

    cmdline = _cached(*args)
    try:
        subprocess.run(cmdline, check=True, **kwargs)
    except subprocess.CalledProcessError as exc:
        msg = f"{args[0]} {args[1]} failed"
        raise ExecutionError(msg) from exc


def check_output(*args: str, **kwargs: dict[str, Any]) -> str:
    """Locate command and run it with the given arguments, returning output"""
    if kwargs.pop("dry_run", False):
        print(" ".join(str(arg) for arg in args))
        return ""

    cmdline = _cached(*args)
    try:
        result = subprocess.check_output(cmdline, stderr=subprocess.STDOUT, **kwargs)
    except subprocess.CalledProcessError as exc:
        msg = f"{args[0]} {args[1]} failed"
        raise ExecutionError(msg) from exc
    return result.decode("ascii")
