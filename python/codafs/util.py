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
""" Various helper commands """

import subprocess
from distutils.spawn import find_executable
from subprocess import DEVNULL

__all__ = ["run", "check_output", "ExecutionError", "DEVNULL"]


class ExecutionError(Exception):
    """Command execution returned and error"""


# cache command path lookups
_COMMAND_CACHE = {}


def _cached(command, *args):
    """locate command and return it path"""
    global _COMMAND_CACHE  # pylint: disable=global-statement
    try:
        return (_COMMAND_CACHE[command],) + args
    except KeyError:
        pass

    filepath = find_executable(command)
    if filepath is None:
        raise FileNotFoundError("Cannot find {}".format(command))
    return (_COMMAND_CACHE.setdefault(command, filepath),) + args


def run(*args, **kwargs):
    """locate command and run it with the given arguments"""
    if kwargs.pop("dry_run", False):
        print(" ".join(str(arg) for arg in args))
        return

    cmdline = _cached(*args)
    try:
        subprocess.run(cmdline, check=True, **kwargs)
    except subprocess.CalledProcessError as exc:
        raise ExecutionError("{} {} failed".format(args[0], args[1])) from exc


def check_output(*args, **kwargs):
    """locate command and run it with the given arguments, returning output"""
    if kwargs.pop("dry_run", False):
        print(" ".join(str(arg) for arg in args))
        return ""

    cmdline = _cached(*args)
    try:
        result = subprocess.check_output(cmdline, stderr=subprocess.STDOUT, **kwargs)
    except subprocess.CalledProcessError as exc:
        raise ExecutionError("{} {} failed".format(args[0], args[1])) from exc
    return result.decode("ascii")
