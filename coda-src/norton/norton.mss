@Man(name="norton", blurb="Coda file server RVM debugger ", chapter="8", sy
stem="Coda Release 2.0")

@BeginCode(SYNOPSIS)
@b(Norton)
@EndCode()

@BeginProse(DESCRIPTION)
@i<Norton> is a utility program that allows you to display Coda file
server structures that are stored in @i<RVM>.  Eventually, @i<Norton>
will be a full special purpose debugger that allows you to manipulate
the structures as well.  @i<Norton>'s command interface uses the gnu
@i<readline> library which features Emacs style command editting as
well as maintaining a command history.  Command completion is also
supported by using @i[<tab>] and @[<esc>-?].

The available commands are:
@option(name="@b[show directory] @i[<volid> <vnum> <unique>]", desc="Lists
the contents of the directory indicated by @i[volid.vmun.unique].
Each entries vnode number and uniquifier are also listed.")

@option(name="@b[delete name] @i[<volid> <vnode> <unique> <file>]",
desc="remove @i<file> from the directory specified by
@i[<volid>.<vnode>.<unique>].  @b[NOTE: delete name does not do
anything to the vnodes associated with @i[<file>], you must remove the
vnodes by hand or update their link count.]")

@option(name="@b[examine] @i[<addr> <len>]", desc="Print <@i[len]>
bytes starting from <@i[addr]> in hex and ascii.")

@option(name="@b[list volumes]", desc="Display a list of all the
volumes on the server.  This list includes the volume index, name,
number, and type.")

@option(name="@b[show free] @i[large | small]", desc="Display all of
the vnodes on either the large or small free vnode list.")

@option(name="@b[show heap]", desc="Display RVM heap usage.")

@option(name="@b[show index] @i[ <volname> | <volid>]", desc="Display
the RVM index of the given volume name or number.")

@option(name="@b[show vnode] @i[<volid> [ <vnode> | ? ] <unique>]",
desc="Show the specified vnode.  If a @i[?] is given rather than a
vnode number, all of the volume's vnode lists are searched for a vnode
with a matching uniquifier.")

@option(name=@b[show volume] @i[<volname> | <volid>]", desc="Show a
summary of the specified volume.")

@option(name=@b[show volume details] @i[<volname> | <volid>]",
desc="Show all of the RVM state of the specified volume.")

@option(name="@b[examine] @i[<addr> <len>]", desc="Print <@i[len]>
bytes starting from <@i[addr]> in hex and ascii.  An alias for
@b[examine].") 
@EndProse()

@BeginProse(AUTHOR)
Joshua Raiff, 1995, Created.
@EndProse()




