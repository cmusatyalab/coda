/* BLURB lgpl

			   Coda File System
			      Release 6

	  Copyright (c) 1987-2003 Carnegie Mellon University
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
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pioctl.h>

#include "codaconf.h"
#include "coda_config.h"
#include "coda.h"

#ifdef __CYGWIN32__
#include <ctype.h>
#include <windows.h>
// XXX should fix the following!
#include "../venus/nt_util.h"

#define MIN(a,b)  ( (a) < (b) ) ? (a) : (b)
#define MAX(a,b)  ( (a) > (b) ) ? (a) : (b)
#endif

#ifdef __cplusplus
}
#endif

static const char *getMountPoint(void)
{
    static const char *mountPoint = NULL;

    if (!mountPoint) {
	codaconf_init("venus.conf");
	CODACONF_STR(mountPoint, "mountpoint", "/coda");
    }
    return mountPoint;
}

#if defined(__CYGWIN32__) // Windows NT and 2000

int marshalling(void **send_data, const char *prefix, const char *path,
		 struct PioctlData *data){
    int total_size, offset_path, offset_in;

    /* begin marshalling inputbuffer */
    /* total_size: structure data, path, out and in buffer from ViceIoctl */
    total_size  = sizeof(struct PioctlData) + strlen(path) + strlen(prefix)
	+ 1 + data->vi.in_size; 

    /* addr in the send_data buffer */
    offset_path = sizeof(struct PioctlData);
    offset_in   = offset_path + strlen(path) + strlen(prefix) + 1;

    /* get buffer */
    *send_data = malloc(total_size);   

    /* copy path in send_data buffer */   
    strcpy(*send_data+offset_path, prefix);
    strcat(*send_data+offset_path, path);

    /* copy the inbuffer from ViceIoctl into the send_data buffer */
    memcpy(*send_data+offset_in, data->vi.in, data->vi.in_size);

    /* copy data in send_data buffer */
    memcpy(*send_data, data, sizeof(struct PioctlData));

    /* end marshalling inputbuffer */

    return total_size;
}

int pioctl(const char *path, unsigned long cmd, 
	   struct ViceIoctl *vidata, int follow) 
{
    /* Pack <path, vidata, follow> into a PioctlData structure 
     * since ioctl takes only one data arg. 
     */
    struct PioctlData data;
    void *send_data;
    int code, res = 0, size;
    DWORD bytesReturned;
    void *outbuf;
    short newlength;
    int  mPlen;
    char prefix [CODA_MAXPATHLEN] = "";
    char *cwd;

    /* Set these only once for each run of a program using pioctl. */
    static HANDLE hdev = NULL;
    char *mountPoint = NULL;
    static char cygdrive [ 15 ] = "/cygdrive/./";
    static char driveletter = 0;
    static char ctl_file [CODA_MAXPATHLEN] = "";

    mountPoint = getMountPoint();
    mPlen = strlen(mountPoint);
    if (mPlen == 2 && mountPoint[1] == ':') {
	driveletter = tolower(mountPoint[0]);
	cygdrive[10] = driveletter;
    }
    // Make a control file name.
    strcpy (ctl_file, mountPoint);
    strcat (ctl_file, "\\");
    strcat (ctl_file, CODA_CONTROL);

    /* Check out the path to see if it is a coda path.  If it is
       a relative path, prefix the path with the working directory. */
    if (!path)
	path = getMountPoint();

    if (strlen(path)) {
	if (driveletter && path[1] == ':' && tolower(path[0]) == driveletter) {
	    path = path+2;
	} else if (strncmp(mountPoint, path, mPlen) == 0) {
	    path = path+mPlen;
	} else if (strncmp("/coda/", path, 6) == 0) {
	    path = path+6;
	} else if (strncmp(cygdrive, path, 12) == 0) {
	    path = path+12;
	} else if (path[0] != '/') {
	    cwd = getwd (NULL);
	    if (strncmp(mountPoint, cwd, mPlen) == 0) {
		strncpy (prefix, cwd+mPlen, CODA_MAXPATHLEN);
	    } else if (strncmp(cygdrive, cwd, 11) == 0) {
		strncpy (prefix, cwd+11, CODA_MAXPATHLEN);
	    } else {
		/* does not look like a coda path! */
		free(cwd);
		errno = ENOENT;
		return -1;
	    }
	    strncat (prefix, "/", CODA_MAXPATHLEN-strlen(prefix));
	    free(cwd);
	} else {
	    // Does not look like a coda file!
	    errno = ENOENT;
	    return -1;
	}
    }

    data.path = path;
    data.follow = follow;
    data.vi = *vidata;
    data.cmd = cmd;

    /* We want to have at least a long in the output buffer. */
    outbuf = malloc (MAX(data.vi.out_size, sizeof(unsigned long)));
    newlength = MAX(data.vi.out_size, sizeof(unsigned long));        

    /* marshalls into send_data. send_data will be allocated.
       make sure to free it afterwards */
    size = marshalling(&send_data, prefix, path, &data);

    /* send_data contains:  PioctlData, path, vi.in
       Returned data comes back through outbuf. */
    
    /* printf ("pioctl: size = %d, cmd = 0x%x, in_size = %d, out_size = %d\n",
       size, cmd, data.vi.in_size, data.vi.out_size); */

    // New try .. on /coda/.CONTROL 
    
    if (!hdev) {
	hdev = CreateFile(ctl_file, 0, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (hdev == INVALID_HANDLE_VALUE){
	    printf("Unable to open .CONTROL file, error %i\n", GetLastError());
	    errno = EIO;
	    return(-1);
	}
    }
    
#if 0
    if (!hdev) {
	hdev = CreateFile("\\\\.\\CODADEV", 0, 0, NULL, OPEN_EXISTING,
			  FILE_FLAG_DELETE_ON_CLOSE, NULL);
	if (hdev == INVALID_HANDLE_VALUE){
	    printf("invalid handle to CODADEV error %i\n", GetLastError());
	    errno = EIO;
	    return(-1);
	}
    }
#endif
    
    code = DeviceIoControl(hdev, CODA_FSCTL_PIOCTL, send_data, size, outbuf,
			   newlength, &bytesReturned, NULL);

    if (!code){
	    res = -1;
	    errno = ENOENT;
    	    printf("DeviceIoControl failed with error %i\n", res*(-1));
    } else{
	    /* copy results in vidatas outbuffer */   
	    memcpy(data.vi.out, outbuf, MIN(newlength, bytesReturned));
    }

    /* if (hdev) CloseHandle(hdev);  -- should close on exit() */

    /* free outbuf, send_data */
    free(send_data);
    free(outbuf);  
    return res;
}


#else /* linux and BSD */


static int _pioctl(const char *path, unsigned long cmd, void *data)
{
    int code, fd;

    fd = open(path, O_RDONLY, 0);
    if (fd < 0) return(fd);

    code = ioctl(fd, cmd, data);

    close(fd);

    return code;
}

int pioctl(const char *path, unsigned long com, 
	   struct ViceIoctl *vidata, int follow) 
{
    /* Pack <path, vidata, follow> into a PioctlData structure 
     * since ioctl takes only one data arg. 
     */
    struct PioctlData data;
    int code;
    static char *ctlfile = NULL;

    /* Must change the size field of the command to match 
       that of the new structure. */
    unsigned long cmd = (com & ~(PIOCPARM_MASK << 16)); /* mask out size  */
    unsigned long size = ((com >> 16) & PIOCPARM_MASK) + sizeof(char *) + sizeof(int);

    cmd	|= (size & PIOCPARM_MASK) << 16;  /* or in corrected size */

    if (!path)
	path = getMountPoint();

    data.path = path;
    data.follow = follow;
    data.vi = *vidata;

    if (!ctlfile) {
	const char *mtpt = getMountPoint();
	ctlfile = malloc(strlen(mtpt) + strlen(CODA_CONTROL) + 2);
	sprintf(ctlfile, "%s/%s", mtpt, CODA_CONTROL);
    }

    code = _pioctl(ctlfile, cmd, &data);

    /* Return result of ioctl. */
    return(code);
}
#endif
