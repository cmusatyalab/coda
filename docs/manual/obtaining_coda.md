# Obtaining Coda

!!! danger

    This page contains very outdated info and needs to be updated and rewritten.

    Slightly more up-to-date info can be found at <http://coda.cs.cmu.edu/mirrors.html>.

As of time of writing, Coda is supported on Linux, NetBSD and FreeBSD. Alpha
quality Windows95/98 Coda client and a WindowsNT Coda server are also available
for testing purposes.  We provide source code tarballs, precompiled Linux
binaries in RPM packages, FreeBSD ports and NetBSD source packages.

!!! tip

    Always check the README file at <https://github.com/cmusatyalab/coda/blob/master/README.md>
    for last minute changes and updates that have not yet found their way into
    the manual.

Here are the specific steps to obtain packaged distributions for supported
platforms:

=== "Linux"

    Download RedHat RPMS for **lwp**, **rvm**, **rpc2**, **coda-debug-client**,
    and, if necessary, **coda-debug-server** from
    <http://coda.cs.cmu.edu/coda/>{fedora,el}/.

=== "FreeBSD"

    Download FreeBSD ports for **lwp**, **rvm**, **rpc2** and **coda** from
    <http://coda.cs.cmu.edu/coda/freebsd/> and untar them into the `/usr/ports`
    directory on your system:

    ``` sh
    # cd /usr/ports
    # tar xvzf <path_to_downloaded_files>/ports-lwp-<version>.tgz
    # tar xvzf <path_to_downloaded_files>/ports-rvm-<version>.tgz
    # tar xvzf <path_to_downloaded_files>/ports-rpc2-<version>.tgz
    # tar xvzf <path_to_downloaded_files>/ports-coda-<version>.tgz
    ```

    !!! important

        FreeBSD ports are actualy only a framework for compilation, they will
        download source tarballs and build Coda on your system.

        If you wish to perform Coda installation at later time, when Internet
        access would not be available, please download tarballs listed above and
        put them into `/usr/ports/distfiles` on your system.

=== "NetBSD"

    Download NetBSD source packages for **lwp**, **rvm**, **rpc2** and **coda**
    from <http://coda.cs.cmu.edu/coda/netbsd/> and untar them into
    `/usr/pkgsrc` directory on your system:

    ``` sh
    # cd /usr/pkgsrc
    # tar xvzf <path_to_downloaded_files>/pkgsrc-lwp-<version>.tgz
    # tar xvzf <path_to_downloaded_files>/pkgsrc-rvm-<version>.tgz
    # tar xvzf <path_to_downloaded_files>/pkgsrc-rpc2-<version>.tgz
    # tar xvzf <path_to_downloaded_files>/pkgsrc-coda-<version>.tgz
    ```

    !!! important

        NetBSD packages are actualy only a framework for compilation, they will
        download source tarballs and build Coda on your system.

        If you wish to perform Coda installation at later time, when Internet
        access would not be available, please download tarballs listed above and
        put them into `/usr/pkgsrc/distfiles` on your system.

=== "Source tarballs"

    You can obtain source code tarballs from <http://coda.cs.cmu.edu/coda/source/>.
