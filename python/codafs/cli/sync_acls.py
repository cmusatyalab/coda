#!/usr/bin/env python3
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
"""coda-sync-acls - Copy Coda ACLs from a source directory tree or a
                 serialized CodaACLs.yaml file.

depends on pyyaml for parsing a CodaACLs.yaml file.
depends on the 'cfs' executable to set the actual ACLs.
optional dependencies on jsonschema (for validation) and tqdm (progressbar)
"""

import argparse
import logging
import sys
from dataclasses import asdict
from operator import itemgetter
from pathlib import Path

import yaml
from codafs.cfs import listacl, setacl
from codafs.optional import ValidationError, jsonschema_validate, tqdm
from codafs.structs import AclEntry
from codafs.walk import walk_volume


def is_directory(path_str: str) -> Path:
    """To make sure the destination argument is a directory"""
    path = Path(path_str)
    if not path.is_dir():
        raise ValueError(f"{path} is not a directory")
    return path


parser = argparse.ArgumentParser()
parser.add_argument(
    "-q",
    "--quiet",
    action="store_true",
    help="only display error messages",
)
parser.add_argument(
    "-v",
    "--verbose",
    action="store_true",
    help="display extra messages",
)
parser.add_argument(
    "-x",
    "--cross-volume",
    action="store_true",
    help="traverse into other Coda volumes",
)
parser.add_argument(
    "-n",
    "--dry-run",
    action="store_true",
    help="avoid running commands that alter state",
)
parser.add_argument(
    "--write-aclfile",
    metavar="CODAACLS_YAML",
    help="write collected acls to a new CodaACLs.yaml file",
)
parser.add_argument(
    "-o",
    "-d",
    "--destination",
    type=is_directory,
    default=".",
    help="destination where ACLs should be updated",
)
parser.add_argument(
    "source",
    metavar="SOURCE",
    type=Path,
    help="directory or 'CodaACLs.yaml' file",
)


# jsonschema for validation
CODA_ACLS_SCHEMA = {
    "$schema": "http://json-schema.org/draft-07/schema#",
    "$id": "http://coda.cs.cmu.edu/schemas/coda_acls.json",
    "$defs": {
        "fspath": {"type": "string"},
        "user_or_group": {"type": "string"},
        "acl_rights": {"type": "string", "pattern": "^-?r?l?i?d?w?k?a?$"},
        "acl_entry": {
            "type": "object",
            "properties": {
                "name": {"$ref": "#/$defs/user_or_group"},
                "rights": {"$ref": "#/$defs/acl_rights"},
            },
            "required": ["name", "rights"],
        },
        "acl": {
            "type": "object",
            "properties": {
                "path": {"$ref": "#/$defs/fspath"},
                "acl": {"type": "array", "items": {"$ref": "#/$defs/acl_entry"}},
            },
            "required": ["path", "acl"],
        },
    },
    "type": "object",
    "properties": {"acls": {"type": "array", "items": {"$ref": "#/$defs/acl"}}},
    "required": ["acls"],
}


def collect_acls(root: Path, cross_volume: bool = False) -> list[AclEntry]:
    """Walk directory tree in Coda, collecting ACLs"""
    directory_acls = []

    if not cross_volume:

        def volume_callback(root, volume_name, _volume_id):
            logging.info("Skipping volume '%s' at %s", volume_name, root)
            raise StopIteration

    else:
        volume_callback = None

    for directory in walk_volume(root, volume_callback=volume_callback):
        logging.info("Getting ACL for %s", directory)

        path = str(directory.relative_to(root))
        acl = [asdict(entry) for entry in listacl(directory)]

        directory_acls.append(dict(path=path, acl=acl))

    return directory_acls


def update_acls(dest: Path, acls: list[AclEntry], dry_run: bool = True):
    """Update Coda ACLs under 'dest'"""
    # sort alphabetically, from leaf to root
    logging.debug("Sorting alphabetically, leaves first")
    acls.sort(key=itemgetter("path"))
    acls.sort(key=lambda acl: acl["path"].count("/"), reverse=True)

    with tqdm(acls, unit=" acls") as pbar:
        for acl in pbar:
            path = dest / acl["path"]

            if not path.exists():
                logging.warning("Skipping non-existent path %s", path)
                continue

            if not path.is_dir():
                logging.warning("Skipping non-directory %s", path)
                continue

            cur_acl = listacl(path)
            new_acl = [(entry["name"], entry["rights"]) for entry in acl["acl"]]

            if cur_acl == new_acl:
                logging.debug("Skipping already set acl at %s", path)
                continue

            setacl(path, new_acl, dry_run=dry_run)


def main() -> int:
    """Copy acls from a source yaml file, or directories in Coda, to
    destination directories in Coda or write to a new yaml file.
    """
    args = parser.parse_args()

    # Set up logging
    logging.basicConfig(
        format="%(levelname)s - %(message)s",
        level=(
            logging.DEBUG
            if args.verbose
            else logging.ERROR if args.quiet else logging.INFO
        ),
    )

    # Ingest ACLs from file or existing directory tree in Coda
    if args.source.is_file():
        with args.source.open() as acl_file:
            logging.info("Loading %s", args.source)
            acl_dict = yaml.safe_load(acl_file)

    elif args.source.is_dir():
        acls = collect_acls(args.source, args.cross_volume)
        acl_dict = dict(acls=acls)

    else:
        logging.error("Source not a file or directory.")
        return 1

    # Validate ACLs against schema
    logging.debug("Validating ACLs read from %s", args.source)
    try:
        jsonschema_validate(acl_dict, CODA_ACLS_SCHEMA)
    except ValidationError as exception:
        print(exception)
        return 1

    acls = acl_dict["acls"]

    # Extract some statistics about the ACLs
    if not args.quiet:
        print("---")
        print("# of acls:", len(acls))

        users = set()
        admins = set()
        for acl in acls:
            for entry in acl["acl"]:
                users.add(entry["name"])
                if "a" in entry["rights"]:
                    admins.add(entry["name"])

        print("Users with some ACL rights:", users)
        print("Users granted admin rights:", admins)
        print("---")

    if args.write_aclfile is not None:
        with Path(args.write_aclfile).expanduser().open("w") as acl_file:
            yaml.dump(dict(acls=acls), acl_file)
    else:
        update_acls(args.destination, acls, dry_run=args.dry_run)
    return 0


if __name__ == "__main__":
    sys.exit(main())
