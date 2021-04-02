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
""" Optional things that are nice to have, but not strictly necessary. """

__all__ = ["jsonschema_validate", "ValidationError", "SchemaError", "tqdm"]

# jsonschema for data validation
try:
    from jsonschema import validate as jsonschema_validate
    from jsonschema.exceptions import SchemaError, ValidationError
except ImportError:

    def jsonschema_validate(_instance, _schema, *_args, **_kwargs):
        """ validation wrapper that doesn't actually validate """

    # we won't actually raise these from our wrapper...
    class ValidationError(Exception):
        """ Failed to validate provided data against jsonschema """

    class SchemaError(Exception):
        """ Failed to validate the provided jsonschema """


# tqdm for a nice progressbar
try:
    from tqdm import tqdm
except ImportError:
    import sys
    from contextlib import AbstractContextManager

    class tqdm(AbstractContextManager):  # pylint: disable=invalid-name
        """ progress bar wrapper that doesn't actually display progress """

        def __init__(self, iterable, *_args, **_kwargs):
            self.iterable = iterable

        def __iter__(self):
            for obj in self.iterable:
                yield obj

        @classmethod
        def write(cls, msg, file=sys.stdout, end="\n", **_kwargs):
            """ write that normally avoids conflicting with the progress bar """
            file.write(msg)
            file.write(end)
