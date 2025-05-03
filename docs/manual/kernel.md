---
title: Kernel Configuration
---
# Configuring Kernels for use with Coda clients

!!! danger
    This page contains very outdated info and needs to be updated and rewritten.

The Coda servers may be run on unmodified kernels.  The Coda **codasrv**
process is user-level code that uses existing kernel services.  The Coda
client, **venus**, however requires some Coda-specific changes be made to the
kernel.  These changes add Coda specific functions to the kernel which are
needed to integrate the Coda file system with the VFS layer in the kernel.

## VFS Interface

In Unix systems, the code which translates user-generated system calls into
file system requests is called the _VFS Interface_.  Coda provides a VFS
Interface by providing the necessary support for systems calls such as creat(),
open(), stat(), etc.

Communication between Venus and the kernel occurs through a character device
`/dev/cfs0` (part of Coda) which provides the kernel with access to the
**venus** cache manager for handling the VFS-to-Coda kernel interface.  Please
see [Installing and Configuring a Coda Client](client_installation.md) for the
major and minor device numbers specific to a supported platform.

The rest of this chapter will focus on integrating the Coda code that satisfies
the Vnode interface into the kernels of supported platforms.

## Configuring a Linux kernel

Coda may be configured as a module or hard-coded in a _monolithic_ kernel.
We have a reasonably flexible method to build kernel modules. You
can build the module for a kernel which you are not running at the
time of the build.

To build a custom coda kernel module you need to obtain the source
from <http://coda.cs.cmu.edu/pub/coda/linux/src/linux-coda-6.8.tar.gz>.

And follow the instructions in the rest of this file.

!!! note

    The code here is always newer than or equal to:

    - the kernel
    - the big Coda tar-ball

## To Build Linux-2.1.x kernels modules

- Patch the kernel:

  For 2.1.10? kernels apply the 106patch.  This will go to Linus
  shortly, so this patch may stop working.

  You must type the following:

  ``` sh
  make oldconfig
  make dep
  make your-type-of-image and modules.
  ```

- In this directory build the coda.o module, type:

  ``` sh
  make config  --- answer the questions
  make coda.o
  su
  make install
  ```

## To Build Linux-2.0.x kernel modules

- Prep the kernel by typing:

  ``` sh
  cd /usr/src/linux-for-you
  make oldconfig
  make dep
  ```

- In this directory build the coda.o module by typing:

  ``` sh
  make config  --- answer the questions
  make coda.o
  su
  make install
  ```

!!! note

    - If you build for a running kernel, you must still have a source
      tree for that Linux release, otherwise the headers cannot be found.
      However, that source tree doesn't need modversions.h etc.

    - We don't actively maintain 2.0 code anymore.  The 2.1 code is
      preferred, but not yet perfect.

## Configuring a FreeBSD kernel

All modern releases of FreeBSD (3.x and 4.x) already include everything
necessary to configure the kernel for a Coda client. To use a Coda client under
FreeBSD you have two options: either use a Coda _loadable kernel module (KLD)_,
or build a custom kernel with Coda support. The first option does not require
significant changes to your installation and can be used on systems without
source code installed.

### Loading Coda kernel module

To experiment with a Coda client, you can load the Coda KLD using following
command: `kldload coda`. To verify that module is loaded run `kldstat`,
`coda.ko` should be listed in its output, which could look similar to this:

    Id Refs Address    Size     Name
     1    6 0xc0100000 1cda44   kernel
     2    1 0xc02ce000 15648    coda.ko
     3    1 0xc0e1a000 7000     procfs.ko
     4    1 0xc0e28000 7000     ipfw.ko
     5    1 0xc0e4a000 15000    linux.ko
     6    1 0xc0e80000 2000     green_saver.ko

The Coda module can be unloaded with `kldunload coda`. To get FreeBSD to load
the Coda module automaticaly on system startup, you should add
_coda\_load="YES"_ to `/boot/loader.conf`.

### Compiling Coda support into a custom kernel

For guidelines on compiling and installing new FreeBSD kernels, please refer to
[configuring the FreeBSD Kernel](http://www.freebsd.org/handbook/kernelconfig.html)
chapter of the FreeBSD Handbook. Coda support requires including following
lines into the kernel configuration file:

    options         CODA                    # CODA filesystem.
    pseudo-device   vcoda   4               # coda minicache <-> venus comm.

This is the only Coda-specific modification requred.

## Configuring a NetBSD kernel

Currently, Coda only works with the 1.3/1.3.x kernels.  Since
a loadable module for Coda is not available, Venus support is compiled
into the kernel.  The Coda Development Group releases a pre-built
GENERIC NetBSD kernel with Coda Venus support.  The only difference
between the GENERIC NetBSD kernel and the Coda kernel is the addition
of Coda File-system.

For those who wish to compile their own kernel, go to
<http://coda.cs.cmu.edu/pub/coda/netbsd/>&lt;OS-VERSION&gt;/i386/

and get the Coda kernel patches:

- `kernel-patch.cfs-4.4.0.gz`
- `kernel-patch.NetBSD-4.4.0-<OS-VERSION>.gz`

You should obtain the NetBSD 1.3/1.32 kernel sources and apply the coda
patches.  Suppose that the kernel source are in `/home/me/mysrc/sys` and the
patch files, `kernel-patch.cfs-4.4.0.gz` and
`kernel-patch.NetBSD-4.4.0-&lt;OS-VERSION&gt;.gz`, are in `/home/me/mysrc`. Go
to `/home/me/mysrc` and extract the Coda patch by typing:

``` sh
zcat kernel-patch.cfs-4.4.0.gz | patch -p4
```

The -p4 is really necessary.  You also need to extract the "glue" code for
Coda.  Type:

``` sh
zcat kernel-patch.NetBSD-4.4.0-<OS-VERSION>.gz | patch -p6
```

At this point, the easiest thing to do is to copy the GENERIC kernel
configuration and modify it to include Coda, i.e.

``` sh
cd .../conf
cp GENERIC Coda
```

The two lines that are need for to the Coda configuration will look something like:

    options                CFS    # Coda File System
    pseudo-device vcoda    4      # coda minicache <-> venus comm.

These should have been added to the GENERIC file by the above patches.
Please verify that they went in correctly.

Now, all that you need to do is to _config_ Coda and build it.

``` sh
config Coda
make
```

Finally, copy the `netbsd` to `/` and reboot.

### Epilog

If you are looking for the above Generic Coda kernel, you can find it in the
NetBSD area on the Coda site.  It is named `netbsd-&lt;RELEASE&gt;.gz`

Don't forget to check _INSTALL_ and _README_ in the Coda NetBSD ftp site for
any last minute changes that may not have found their way into the Coda manual.
Also, remember you only need a modified kernel for the client side of Coda, not
the server side.
