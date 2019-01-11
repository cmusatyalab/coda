/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifdef __CYGWIN32__
#include <windows.h>
#include <winbase.h>
#include <winioctl.h>

// Defines needed for DeviceIoControls from OSR headers

#define OW_FSCTL_MOUNT_PSEUDO \
    CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 3031, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define OW_FSCTL_DISMOUNT_PSEUDO \
    CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 3033, METHOD_NEITHER, FILE_WRITE_ACCESS)

//
// Pseudo mount/dismount data structures
//

typedef struct {
    HANDLE PseudoVolumeHandle;
    USHORT PseudoDeviceNameLength;
    PWCHAR PseudoDeviceName;
    USHORT PseudoLinkNameLength;
    PWCHAR PseudoLinkName;
} OW_PSEUDO_MOUNT_INFO, *POW_PSEUDO_MOUNT_INFO;

// XXX broken ... should use CTL_CODE like above.. should match
// ntfsd/coda/coda_nt.h
//
#define CODA_FSCTL_ANSWER 0x1002
#define CODA_FSCTL_FETCH 0x1003
#define CODA_FSCTL_PIOCTL 0x1006

// prototypes

void nt_mount(const char *drivename);
void nt_umount(const char *drivename);

int nt_initialize_ipc(int sock);
int nt_msg_write(const char *buf, int size);
void nt_stop_ipc(void);

#endif
