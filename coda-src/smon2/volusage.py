#!/usr/bin/python

import sys, popen2, string

if not sys.argv[1:]:
  print sys.argv[0], "<host> [volutil-cmd]"

host = sys.argv[1]

volutil = "volutil"
if sys.argv[2:]:
  volutil = sys.argv[2]

fin, fout = popen2.popen2('%s -h %s getvolumelist' % (volutil, host))
fout.close()

vollist = []

while 1:
  l = string.strip(fin.readline())
  if not l: break

  parts = string.split(l)
  if len(parts) < 7: continue

  volume = parts[0][1:]
  usage = long(parts[6][1:], 16)
  partition = parts[3][1:]
  activity = long(parts[11][1:], 16)
  
  vollist.append((partition, usage, volume, activity))

vollist.sort()

for partition, usage, volume, activity in vollist:
  print "%s\t%-40s\t%6d\t%6d" % (partition, volume, usage, activity)

