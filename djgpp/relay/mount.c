/*
   Copyright 1997-98 Michael Callahan
   This program is free software.  You may copy it according
   to the conditions of the GNU General Public License version 2.0
   or later; see the file COPYING in the source directory.
*/

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

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

extern int __quiet_socket;

main(int argc, char *argv[])
{
    char mountstring[] = "Z\\\\CODA\\CLUSTER";
    int res;

    if ((argc != 2) || 
	(strlen(argv[1]) != 2) ||
	(argv[1][1] != ':')) {
	printf ("usage: %s <driveletter>:\n", argv[0]);
	exit (1);
    }

    mountstring[0] = toupper(argv[1][0]);

    printf ("Mounting on %c:\n", mountstring[0]);

    if (!open_vxd("CODADEV ", &codadev_api)) {
	printf ("CODADEV not there!\n");
	exit (1);
    }

    res = DeviceIoControl(2, mountstring, strlen(mountstring), NULL, 0);
    if (res) {
	printf ("Mount failed: %d\n", res);
	exit (1);
    }

    printf ("Mount OK\n");
}


