#!/usr/bin/python3

import argparse
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("host")
parser.add_argument("--volutil", default="volutil", help="Path to volutil command")
args = parser.parse_args()


stdout = subprocess.check_output(
    [args.volutil, "-h", args.host, "getvolumelist"], universal_newlines=True
)

vollist = []

for line in stdout.split("\n"):
    parts = line.split()
    if len(parts) < 7:
        continue

    volume = parts[0][1:]
    usage = int(parts[6][1:], 16)
    partition = parts[3][1:]
    activity = int(parts[11][1:], 16)

    vollist.append((partition, usage, volume, activity))

vollist.sort()

for partition, usage, volume, activity in vollist:
    print("{}\t{:40}\t{:8}\t{:6}".format(partition, volume, usage, activity))
