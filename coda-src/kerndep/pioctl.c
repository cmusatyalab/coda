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
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <pioctl.h>

#if defined(__CYGWIN32__) || defined(DJGPP)

#ifdef __CYGWIN32__
#include <ctype.h>
#include <windows.h>
#include "coda.h"
#include "codaconf.h"    
#include "coda_config.h"
// XXX should fix the following!
#include "../venus/nt_util.h"
#endif

#define MIN(a,b)  ( (a) < (b) ) ? (a) : (b)
#define MAX(a,b)  ( (a) > (b) ) ? (a) : (b)

#endif

#ifdef __cplusplus
}
#endif

// Windows 95/98

#ifdef DJGPP
//#include <auth2.h>
struct far_ptr {
  unsigned int offset; 
  unsigned short segment;
};

struct far_ptr codadev_api = {0,0};

int open_vxd(char *vxdname, struct far_ptr *api)
{
  asm ("pushw  %%es\n\t"
       "movw   %%ds, %%ax\n\t"
       "movw   %%ax, %%es\n\t"
       "movw   $0x1684, %%ax\n\t"
       "int    $0x2f\n\t"
       "movw   %%es, %%ax\n\t"
       "popw   %%es\n\t"
       : "=a" (api->segment), "=D" (api->offset) 
       : "b" (0), "D" (vxdname));
  api->offset &= 0xffff;
  return api->offset != 0;
}

int DeviceIoControl(int func, void *inBuf, int inCount,
		    void *outBuf, int outCount)
{
  unsigned int err;

  if (codadev_api.offset == 0)
    return -1;

  
  asm ("lcall  %1"
       : "=a" (err)
       : "m" (codadev_api), "a" (func), "S" (inBuf), "b" (inCount),
       "D" (outBuf), "c" (outCount), "d" (123456));
  if (err)
    printf ("DeviceIoControl %d: err %d\n", func, err);
  return err;
}

/*
typedef struct {
    int			    sTokenSize;
    EncryptedSecretToken    stoken;
    int			    cTokenSize;
    ClearToken		    ctoken;
    } venusbuff;*/

int marschalling(void **send_data, const char *path, struct PioctlData *data){
    int total_size, offset_path, offset_out, offset_in;
    void *save_out;

    /* begin marschalling inputbuffer */
    /* total_size: structure data, path, out and in buffer from ViceIoctl */
    total_size  = sizeof(struct PioctlData)+strlen(path)+1+data->vi.out_size+data->vi.in_size; 

    /* addr in the send_data buffer */
    offset_path = sizeof(struct PioctlData);
    offset_out  = offset_path + strlen(path) + 1;
    offset_in   = offset_out + data->vi.out_size;

    /* get buffer */
    *send_data = malloc(total_size);   

    /* copy the outbuffer from ViceIoctl into the send_data buffer */    
    memcpy(*send_data+offset_out, data->vi.out, data->vi.out_size);

    /* copy the inbuffer from ViceIoctl into the send_data buffer */
    memcpy(*send_data+offset_in, data->vi.in, data->vi.in_size);

    /* copy data in send_data buffer */
    memcpy(*send_data, data, sizeof(struct PioctlData));

    /* copy path in send_data buffer */   
    memcpy(*send_data+offset_path, path, strlen(path)+1);
    /* end marschalling inputbuffer */

    return total_size;
}

int pioctl(const char *path, unsigned long com, 
	   struct ViceIoctl *vidata, int follow) 
{
    /* Pack <path, vidata, follow> into a PioctlData structure 
     * since ioctl takes only one data arg. 
     */
    struct PioctlData data;
    void *send_data;
    int code, res = 0, size;
    HANDLE hdev = NULL;
    DWORD bytesReturned;
    char *s;
    void *outbuf;
    short newlength;
    unsigned long cmd;   

    /* Must change the size field of the command to match 
       that of the new structure. */
    cmd = (com & ~(PIOCPARM_MASK << 16)); /* mask out size  */
    size = ((com >> 16) & PIOCPARM_MASK) + sizeof(char *) + sizeof(int);
    cmd	|= (size & PIOCPARM_MASK) << 16;  /* or in corrected size */

    s = index(path, ':');
    if (s) path = s+1;

//    printf("pioctl: path = '%s'\n", path);
   
    data.path = path;
    data.follow = follow;
    data.vi = *vidata;

    /* outbuf uses the outbuffer of IOControl to pass in the pioctl cmd */
    /* we can't override the vidata outbuffer, because this might contain data */
    /* which has created and is needed by the calling process. Upon return */
    /* the results are copied from outbuf into vidatas outbuffer */
    outbuf = malloc (MAX(data.vi.out_size, sizeof(unsigned long)));
    memcpy(outbuf, &cmd, sizeof(unsigned long));
    newlength = MAX(data.vi.out_size, sizeof(unsigned long));        

    /* marschalls into send_data. send_data will be allocated. make sure to free it afterwards*/ 
    size = marschalling(&send_data, path, &data); 

    /* send_data contains blob of PioctlData, path, vi.out, vi.in; */
    /* this is neccessary for DJGPP. Also used for Cygwin to keep kernel module clean */
    /* outbuf contains pioctl command and will be used for return values */
    
    if (!open_vxd("CODADEV ", &codadev_api)) {	   
	    int version;
	    if (!open_vxdldr()){
		    printf("cannot load VXDLDR\n");
		    return -1;
	    }
	    if (!vxdldr_get_version(&version)) {
		    printf ("cannot get VXDLDR version\n");
		    return -1;
	    }
	    if ((res = vxdldr_load_device("codadev.vxd"))) {
		    printf ("cannot load CODADEV: VXDLDR error %d\n", res);
		    return -1;
	    }
	    if (!open_vxd("CODADEV ", &codadev_api)) {
		    printf ("Loaded CODADEV, but could not get api\n");
		    return -1;
	    }
    }
    /*    printf("%i\n", ((venusbuff *)vidata->in)->ctoken.EndTimestamp);*/
    /* printf("test : %s, path : %s size : %i ref : %i\n", send_data+sizeof(struct PioctlData), 
       path, size, sizeof(struct PioctlData));*/
    
    code = DeviceIoControl(11, (void *)send_data , size, outbuf, newlength);
    if (code)
	    res = code * (-1);
    else
	    /* copy results in vidatas outbuffer */
	    memcpy(data.vi.out, outbuf, MIN(newlength, data.vi.out_size)); 
    
    /* free outbuf, send_data */
    free(send_data);
    free(outbuf);  
    return res;
}

#else
#if defined(__CYGWIN32__)

// Windows NT and 2000 ... different enough from the Windows 95/98 to be
// in a complete separate block of code.


int marschalling(void **send_data, const char *prefix, const char *path,
		 struct PioctlData *data){
    int total_size, offset_path, offset_in;

    /* begin marschalling inputbuffer */
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

    /* end marschalling inputbuffer */

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
    char *s;
    void *outbuf;
    short newlength;
    int  mPlen;
    char prefix [CODA_MAXPATHLEN] = "";
    char *cwd;

    /* Set these only once for each run of a program using pioctl. */ 
    static HANDLE hdev = NULL;
    static char *mountPoint = NULL;
    static char cygdrive [ 15 ] = "/cygdrive/./";
    static char driveletter = 0;


    /*  Do this only once for each execution. */

    if (!mountPoint) {
	conf_init(SYSCONFDIR "/venus.conf");
	CONF_STR(mountPoint, "mountpoint", "/coda");
	mPlen = strlen(mountPoint);
	if (mPlen == 2 && mountPoint[1] == ':') {
	    driveletter = tolower(mountPoint[0]);
	    cygdrive[10] = driveletter;
	}
    }

    /* Check out the path to see if it is a coda path.  If it is
       a relative path, prefix the path with the working directory. */

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

    /* marschalls into send_data. send_data will be allocated.
       make sure to free it afterwards */
    size = marschalling(&send_data, prefix, path, &data); 

    /* send_data contains:  PioctlData, path, vi.in
       Returned data comes back through outbuf. */
    
    /* printf ("pioctl: size = %d, cmd = 0x%x, in_size = %d, out_size = %d\n",
       size, cmd, data.vi.in_size, data.vi.out_size); */

    if (!hdev) {
	hdev = CreateFile("\\\\.\\CODADEV", 0, 0, NULL, OPEN_EXISTING,
			  FILE_FLAG_DELETE_ON_CLOSE, NULL);
	if (hdev == INVALID_HANDLE_VALUE){
	    printf("invalid handle to CODADEV error %i\n", GetLastError());
	    errno = EIO;
	    return(-1);
	}
    }
    
    code = DeviceIoControl(hdev, CODA_FSCTL_PIOCTL, send_data, size, outbuf,
			   newlength, &bytesReturned, NULL);

    if (!code){
	    res = -1;
	    errno = ENOENT;
    //	    printf("DeviceIoControl failed with error %i\n", res*(-1));
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


static int _pioctl(const char *path, int cmd, void *data)
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

    /* Must change the size field of the command to match 
       that of the new structure. */
    unsigned long cmd = (com & ~(PIOCPARM_MASK << 16)); /* mask out size  */
    int	size = ((com >> 16) & PIOCPARM_MASK) + sizeof(char *) + sizeof(int);

    cmd	|= (size & PIOCPARM_MASK) << 16;  /* or in corrected size */

    data.path = path;
    data.follow = follow;
    data.vi = *vidata;

    code = _pioctl(CTL_FILE, cmd, &data);

    /* Return result of ioctl. */
    return(code);
}

#endif
#endif
