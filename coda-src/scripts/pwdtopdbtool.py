#!/usr/bin/python

import sys, string, os

#************** these will probably need to be configured
# the `System' user initially owns all groups.
groupowner = "System"
groupownid = 777
oldusers   = "/vice/db/user.coda"
oldgroups  = "/vice/db/group.coda"
#**************

# Remove existing pdbtool databases, extreme, but simple
if os.path.exists('/vice/db/prot_users.db'):
  os.unlink('/vice/db/prot_users.db')
  os.unlink('/vice/db/prot_index.db')

# Start a pipe to the pdbtool
pdbtool = os.popen('pdbtool', 'w')

# Whack in a couple of the default entries
pdbtool.write("nui %(groupowner)s %(groupownid)d\n" % vars())
pdbtool.write("ng System:AnyUser %(groupownid)d\n" % vars())
pdbtool.write("ng System:Administrators %(groupownid)d\n" % vars())

# Add all existing users
names = {}
for line in open(oldusers).readlines():
  (name, x, uid, y, fullname) = string.splitfields(line, ':')[:5]
  names[name] = uid
  pdbtool.write("nui %(name)s %(uid)s\n" % vars())
  
# Add all existing groups
for line in open(oldgroups).readlines():
  group, gid = string.split(line)[:2]
  members   = string.split(line)[2:]
  gid = string.atoi(gid)
  pdbtool.write("ng %(group)s %(groupownid)d\n" % vars())
  pdbtool.write("ci %(group)s %(gid)d\n" % vars())
  for user in members:
    uid = names[user]
    pdbtool.write("ag %(gid)s %(uid)s\n" % vars())

pdbtool.close()
print

