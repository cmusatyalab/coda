# User Administration

As of Coda-5.2.x the user and group administration tools and databases have
been replaced. Now, instead of editing `user.coda` and `group.coda`, and then
converting them with **pwdtopdb** and **pcfgen** has been replaced by the new
**pdbtool** program.

## Short introduction to pdbtool

The **pdbtool** command is an interactive command to manipulate and query user
and group information. The most commonly used commands in **pdbtool** are:

- `nui <username> <userid>` - create a new user, with the specified id.
- `ng <groupname> <ownerid>` - create a new group, with the specified owner.
- `ci <user/groupname> <newid>` - change the id of an existing user or group.
- `ag <group-id> <user/groupid>` - add a user/group to a group.
- `i <user/groupname>` - list all information about the user or group.

!!! note

    User-ids are supposed to be positive integers, group-ids are negative
    integers.

For more information read the manualpage ([pdbtool.8](../manpages/pdbtool.8.md)),
or simply play around with **pdbtool**'s **help** command.

## Adding a new user

This is a step-by-step example of the administrative steps involved in adding a
new user to Coda. Here we assume that the host named "scm" is one that has the
read-write copy of the databases.  The new user is going to be added to the
groups Users and Developers.

``` sh
root@scm# pdbtool
pdbtool> nui jan 768
pdbtool> i jan
USER jan
  *  id: 768
  *  belongs to no groups
  *  cps: [ 768 ]
  *  owns no groups
pdbtool> i Users
GROUP Users OWNED BY System
  *  id: -221
  *  owner id: 777
  *  belongs to no groups
  *  cps: [ -221 ]
  *  has members: [ 22 178 184 303 545 697 822 823 835 894 712 738 777 901 902 ]
pdbtool> i Developers
GROUP Developers OWNED BY System
  *  id: -225
  *  owner id: 777
  *  belongs to no groups
  *  cps: [ -225 ]
  *  has members: [ 122 835 ]
pdbtool> ag -221 768
pdbtool> ag -225 768
pdbtool> i jan
USER jan
  *  id: 768
  *  belongs to groups: [ -221 -225 ]
  *  cps: [ -221 -225 768 ]
  *  owns no groups
pdbtool> q
```

This sequence has created the new user account, and added the account to the
appropriate groups. Now in order to activate the account, we need to set an
initial password with the authentication server.

``` sh
admin@anymachine$ au -h <scm> nu
Your Vice Name: codaadmin
Your Vice Password: ********
New User Name: jan
New User Password: newpassword
```

To finish up, we can create a home volume, mount it, set the ACLs, and the user
is set up. So all the user needs to do is change his password.

``` sh
root@scm# createvol_rep users:jan E0000100 /vicepa
admin@anymachine$ cfs mkm /coda/usr/jan users:jan
admin@anymachine$ cfs sa /coda/usr/jan jan all
jan@anymachine$ cpasswd -h scm
```

## Upgrading from `user.coda`/`group.coda`

To upgrade the user and group databases to the new format, a script called
**pwdtopdbtool.py** has been added to the distribution. You might need to alter
the pathnames of `/vice/db/user.coda` and `/vice/db/group.coda`. Simply run
this script on the SCM and it will parse out all entries from the old user and
group files, and uses the pdbtool to create the new `prot_users.db` and
`prot_index.db` files in `/vice/db`.

To distribute these files to the other servers, you should add the following
lines to `/vice/db/files`:

    prot_users.db
    prot_index.db

As long as there are pre-5.2 servers in the group, it is advised to stick to
the old way of adding users, and use the **pwdtopdbtool.py** script to keep the
new files in sync.

## Upgrading from `coda.pdb`/`name.pdb`

The initial release using the pdbtools used gdbm as the underlying data store.
However we were unable to reliably use the resulting databases across
heterogeneous servers. So we decided to switch to libdb 1.85.

There is no script available to automatically upgrade your gdbm databases to
libdb 1.85 format. If you still have existing `users.coda`/`groups.coda`,
simply run **pwdtopdbtool.py**.

Otherwise, you must extract all information using the old pdbtool
*before* upgrading your SCM.

``` sh
# echo list | pdbtool > full_pdb_dump
```

However, it is not possible to easily use this dump to recreate the database.
Everything needs to be re-added to the new pdb database by hand.
