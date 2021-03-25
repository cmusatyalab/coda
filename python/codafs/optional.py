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

    # we won't raise these from our wrapper, they don't actually need stuff
    class ValidationError(Exception):
        pass

    class SchemaError(Exception):
        pass


# tqdm for a nice progressbar
try:
    from tqdm import tqdm
except ImportError:
    from contextlib import contextmanager

    @contextmanager
    def tqdm(iterable, *_args, **_kwargs):
        """ progress bar wrapper that doesn't actually display progress """
        for item in iterable:
            yield item
