/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/


#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <pioctl.h>

#ifdef __CYGWIN32__
#include <windows.h>
#endif

#ifdef __cplusplus
}
#endif __cplusplus

#ifdef __CYGWIN32__
int pioctl(const char *path, unsigned long com, 
	   struct ViceIoctl *vidata, int follow) 
{
    /* Pack <path, vidata, follow> into a PioctlData structure 
     * since ioctl takes only one data arg. 
     */
    struct PioctlData data;
    int code, res = 0;
    HANDLE hdev;
    DWORD bytesReturned;
    char *s;

    /* Must change the size field of the command to match 
       that of the new structure. */
    unsigned long cmd = (com & ~(PIOCPARM_MASK << 16)); /* mask out size  */
    int	size = ((com >> 16) & PIOCPARM_MASK) + sizeof(char *) + sizeof(int);
    cmd	|= (size & PIOCPARM_MASK) << 16;  /* or in corrected size */

    /*    if (index(path, '/')) return (-1); */
    s = index(path, ':');
    if (s) path = s+1;
    /*    if (!strncmp(path, "\\", 1)||!strncmp(path, "/", 1)) path++;  */
    printf("path : %s\n", path);
   
    data.cmd = cmd;
    data.path = path;
    data.follow = follow;
    data.vi = *vidata;
    /*   printf("insize: %i\n", data.vi.in_size);   */

    hdev = CreateFile("\\\\.\\CODADEV", 0, 0, NULL, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (hdev == INVALID_HANDLE_VALUE){
	    printf("invalid handle to CODADEV error %i\n", GetLastError());
	    return(-1);
    }

    code = DeviceIoControl(hdev, 11, (LPVOID)&data, sizeof(struct PioctlData),
			   vidata->out, vidata->out_size, &bytesReturned, NULL);
   

    if (!code){
	    res = GetLastError()*(-1);
	    printf("DeviceIoControl failed with error %i\n", res*(-1));
    }

    CloseHandle(hdev);
    return res;
}
#else
int pioctl(const char *path, unsigned long com, 
	   struct ViceIoctl *vidata, int follow) 
{
    /* Pack <path, vidata, follow> into a PioctlData structure 
     * since ioctl takes only one data arg. 
     */
    struct PioctlData data;
    int code, fd;

    /* Must change the size field of the command to match 
       that of the new structure. */
    unsigned long cmd = (com & ~(PIOCPARM_MASK << 16)); /* mask out size  */
    int	size = ((com >> 16) & PIOCPARM_MASK) + sizeof(char *) + sizeof(int);

    cmd	|= (size & PIOCPARM_MASK) << 16;  /* or in corrected size */

    data.path = path;
    data.follow = follow;
    data.vi = *vidata;

    fd = open(CTL_FILE, O_RDONLY, 0);
    if (fd < 0) return(fd);

    code = ioctl(fd, cmd, &data);

    (void)close(fd);

    /* Return result of ioctl. */
    return(code);
}
#endif
