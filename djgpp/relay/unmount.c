/*
   Copyright 1997-98 Michael Callahan
   This program is free software.  You may copy it according
   to the conditions of the GNU General Public License version 2.0
   or later; see the file COPYING in the source directory.
*/


#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <stdio.h>
#include <errno.h>


#include <cfs/coda.h>

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

main()
{
  int udpfd;
  int res;
  struct sockaddr_in addr;
  union outputArgs out;

  __quiet_socket = 1;

  udpfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (udpfd == -1) {
    perror ("socket");
    exit (1);
  }

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = 0;
  if (bind(udpfd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
    perror ("bind");
    exit (1);
  }

  if (!open_vxd("CODADEV ", &codadev_api)) {
    printf ("CODADEV not there!\n");
    exit (1);
  }

  res = DeviceIoControl(8, NULL, 0, NULL, 0);
  if (res) {
    printf ("Unmount failed: %d\n", res);
    exit (1);
  }

  printf ("Unmount OK\n");

  out.oh.opcode = 99;
  addr.sin_addr.s_addr = htonl (0x7f000001);
  addr.sin_port = htons(8001); 
  res = sendto(udpfd, (char *) &out, sizeof(out), 0,
	       (struct sockaddr *) &addr, sizeof(addr));
  if (res == -1)
    perror ("sendto");
}


