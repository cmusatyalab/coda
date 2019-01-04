# Coda Distributed File System [![Build Status](https://travis-ci.org/cmusatyalab/coda.svg?branch=master)](https://travis-ci.org/cmusatyalab/coda)

Coda is an advanced networked filesystem. It has been developed at CMU since
1987 by the systems group of [M. Satyanarayanan](http://www.cs.cmu.edu/~satya)
in the SCS department.

### About this repository

This repository combines, to a best effort, the history from the official CVS
repositories of Coda, as well as the supporting LWP, RPC2 and RVM libraries.
The CVS repositories at this point only useful as a historical reference and
further development will only be committed to the Git repository. The
supporting libraries can be found in their respective directories under
`lib-src/`.

The reasons the supporting libraries and their history have been merged with the
Coda source tree are twofold. They were originally part of the Coda source
tree but were seperated out as over time other non-Coda users of the libraries
emerged. By now Coda has outlived all other external users and merging them
back to consolidate the development history seemed to make more sense.

For the past several Coda releases there have been new releases of the LWP,
RPC2 and RVM libraries. Coordinating building and packaging of a single source
tree is more straightforward than building four different ones, especially when
there is a specific build/install ordering dependency and a new build could
break a currently installed version. The current combined tree will ensure that
everything builds and installs with the same library versions.

## BUILDING CODA

### Dependencies

Coda requires a working C/C++ development environment with gcc, gcc-c++,
autotools, libtool, automake, pkg-config, flex, and bison. We require
development headers for readline, ncurses5, and optionally the lua5.1
library.

On Redhat/Fedora/CentOS systems
```sh
$ yum install gcc gcc-c++ autoconf automake libtool pkgconfig flex bison readline-devel ncurses5-devel lua-devel clang
```

On Debian/Ubuntu and derived systems
```sh
$ apt-get install build-essential automake libtool pkg-config flex bison libreadline-dev libncurses5-dev liblua5.1-0-dev clang-format-6.0 valgrind
```

### Build

```sh
$ ./bootstrap.sh
$ ./configure --prefix=/usr --with-lua
$ make
$ sudo make install
```


## RUNNING CODA

### Configuring the Coda client

You need a configuration file in `/etc/coda/venus.conf`. The quickest way to
get started is to run `sudo venus-setup` which will copy an example file with a
suggested configuration, it will also create the directories that Coda uses for
the local file cache and logs.

The first time the Coda client is started we need to force it to clean and
initialize its 'recoverable virtual memory' (persistent state), this is done by
running `sudo venus -init`.

### Running the Coda client

The Coda client needs access to the file system services provided by the 'coda'
kernel module, so to successfully start the client we have to make sure this
module is loaded first. Once the kernel module is available, the client should
be able to start and will automatically start running in the background as soon
as it successfully mounts the file system on `/coda`.

```sh
$ sudo modprobe coda
$ sudo venus
```

Initially nothing will be visible under the `/coda` mountpoint, however any
lookup for a Coda realm name, either through the file system, or through
obtaining Coda credentials, will lead to a Coda server discovery request that,
when successful, will make a permanent realm root appear.

```sh
$ ls /coda/testserver.coda.cs.cmu.edu
$ clog guest@testserver.coda.cs.cmu.edu
password: guest
$ ls /coda
testserver.coda.cs.cmu.edu
```

Because Coda persists its state in recoverable memory, the root will be visible
even when Coda is restarted. To cleanly shut down the system, first unmount the
file system and only then stop the daemon process, this way if any process has
open references to files in Coda the unmount will fail and we avoid lost data.

```sh
$ sudo umount /coda && sudo killall venus
```

### Configuring the Coda server

TBD


## DEVELOPMENT

Although the source of LWP, RPC2, and RVM has been merged back into the main
Coda repository, we are still trying to keep them mostly independent. This
means that they retain their own build infrastructure and library versioning.

When updating supporting library sources make sure to properly follow the
libtool library versioning guidelines. The version info is set using the
CODA_LIBRARY_VERSION macro in configure.ac and consists of
`current:revision:age` values which update according to the following basic
rules.

- If the library source code has changed at all since the last update, then
  increment revision (‘c:r:a’ becomes ‘c:r+1:a’)
- If any interfaces have been added, removed, or changed since the last update,
  increment current, and set revision to 0.
- If any interfaces have been added since the last public release, then
  increment age.
- If any interfaces have been removed or changed since the last public release,
  then set age to 0.

Aside from this, we try to minimize overall version number changes so we skip
updating if the corresponding version number change has already been applied
since the last tagged stable release. This is different from libtool guidelines
where they suggest updating only immediately before a public release.

The assumption here is that version updates are easily forgotten right before a
release so it is better to update early. Developers also don't have to care as
much because they can run their binaries directly from the build tree in which
case libtool will make sure the right library is used, or will use `make install`
which updates everything at the same time.
