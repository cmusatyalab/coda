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
"""Optional things that are nice to have, but not strictly necessary."""

__all__ = ["SchemaError", "ValidationError", "jsonschema_validate", "tqdm"]

import logging

# jsonschema for data validation
try:
    from jsonschema import draft7_format_checker, validate
    from jsonschema.exceptions import SchemaError, ValidationError

    try:
        import ipaddress

        @draft7_format_checker.checks(
            "ipv4network",
            (ipaddress.AddressValueError, ipaddress.NetmaskValueError),
        )
        def is_ipv4network(instance):
            if not isinstance(instance, str):
                return True
            return ipaddress.IPv4Network(instance)

    except ImportError:
        logging.debug("missing ipaddress module, not validating ipv4 network addresses")

    def jsonschema_validate(instance, schema, *args, **kwargs):
        validate(
            instance,
            schema,
            *args,
            format_checker=draft7_format_checker,
            **kwargs,
        )

except ImportError:

    def jsonschema_validate(_instance, _schema, *_args, **_kwargs):
        """Validation wrapper that doesn't actually validate"""

    # we won't actually raise these from our wrapper...
    class ValidationError(Exception):
        """Failed to validate provided data against jsonschema"""

    class SchemaError(Exception):
        """Failed to validate the provided jsonschema"""


# tqdm for a nice progressbar
try:
    from tqdm import tqdm
except ImportError:
    import sys

    class tqdm:  # pylint: disable=invalid-name
        """progress bar wrapper that doesn't actually display progress"""

        def __init__(self, iterable, *_args, **_kwargs):
            self.iterable = iterable

        def __enter__(self):
            return self

        def __exit__(self, _exc_type, _exc_value, _traceback):
            return None

        def __iter__(self):
            yield from self.iterable

        @classmethod
        def write(cls, msg, file=sys.stdout, end="\n", **_kwargs):
            """Write that normally avoids conflicting with the progress bar"""
            file.write(msg)
            file.write(end)
