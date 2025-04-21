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
"""volmunge - Walk a Coda tree to resolve conflicts, identify mountpoints, etc."""

import argparse
import logging
import os
import sys
from pathlib import Path

from codafs.optional import tqdm
from codafs.walk import walk_volumes

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
parser.add_argument("-a", "--all", action="store_true", help="print all paths")
parser.add_argument(
    "-d",
    "--directories",
    action="store_true",
    help="find all directories",
)
parser.add_argument("-f", "--files", action="store_true", help="find all files")
parser.add_argument("-l", "--links", action="store_true", help="find all symlinks")
parser.add_argument(
    "-i",
    "--inconsistent",
    action="store_true",
    help="find inconsistent objects (conflicts)",
)
parser.add_argument(
    "-m",
    "--mountpoints",
    action="store_true",
    help="find volume mount points",
)
parser.add_argument(
    "-u",
    "--mountlinks",
    action="store_true",
    help="find unmounted volume mounts (mountlinks)",
)
parser.add_argument(
    "-o",
    "--open-files",
    action="store_true",
    help="open all files (forces fetch operation)",
)
parser.add_argument(
    "-z",
    "--is-empty",
    action="store_true",
    help="find empty files",
)
parser.add_argument(
    "root",
    metavar="PATH",
    nargs="+",
    type=Path,
    help="root of directory tree to traverse",
)


def main() -> int:
    """Iterate through a volume in Coda."""
    args = parser.parse_args()

    if args.all:
        args.directories = args.files = args.links = True
        args.mountpoints = args.inconsistent = args.mountlinks = True

    # Set up logging
    logging.basicConfig(
        format="%(levelname)s - %(message)s",
        level=(
            logging.DEBUG
            if args.verbose
            else logging.ERROR if args.quiet else logging.INFO
        ),
    )

    def volume_callback(
        root,
        volume_name,
        _volume_id,
        mountpoints=args.mountpoints,
        cross_volume=args.cross_volume,
    ):
        if mountpoints:
            tqdm.write(f"M {root} ({volume_name})")
        if not cross_volume:
            raise StopIteration

    # walk the directory tree
    try:
        for directory in tqdm(
            walk_volumes(args.root, volume_callback=volume_callback),
            unit=" dirs",
            disable=None,
        ):
            if args.directories:
                tqdm.write(f"D {directory}")

            # yeah, we end up iterating each directory twice
            # hopefully Coda's caching helps here...
            try:
                for path in directory.iterdir():
                    # directories and mountpoints are handled by walk_volume
                    if path.is_file():
                        if args.files:
                            tqdm.write(f"F {path}")
                        if args.is_empty and path.stat().st_size == 0:
                            tqdm.write(f"Z {path}")
                        if args.open_files:
                            with open(path) as _:
                                pass
                    elif path.is_symlink():
                        # Normal symlinks have mode 0777
                        # Coda's mount/conflict links have mode 0755
                        if not path.lstat().st_mode & 0o033:
                            link = os.readlink(str(path))
                            if link[0] == "@":
                                if args.inconsistent:
                                    tqdm.write(f"I {path}")
                            elif link[0] == "#":
                                if args.mountlinks:
                                    tqdm.write(f"U {path}")
                            else:
                                logging.critical(
                                    "Unknown mountlink at %s (%s)",
                                    path,
                                    link,
                                )
                        elif args.links:
                            tqdm.write(f"L {path}")

                if args.is_empty:
                    for _ in directory.iterdir():
                        break
                    else:
                        tqdm.write(f"Z {directory}/")

            except InterruptedError as e:
                tqdm.write(str(e))
                continue
            except PermissionError:
                continue
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
