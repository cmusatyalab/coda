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

#include "vxd.h"

struct far_ptr codadev_api = {0,0};

extern int __quiet_socket;

main(int argc, char *argv[])
{
  int udpfd;
  int res;
  struct sockaddr_in addr;
  union outputArgs out; 
  char mountstring[] = "X:";

  __quiet_socket = 1;
 
  if ((argc != 2) || (strlen(argv[1]) != 2) || (argv[1][1] != ':')) {
	printf ("usage: %s <driveletter>:\n", argv[0]);
	exit (1);
  }
  mountstring[0] = toupper(argv[1][0]);

  /*  udpfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
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
  */
  if (!open_vxd("CODADEV ", &codadev_api)) {
    printf ("CODADEV not there!\n");
    exit (1);
  }

  res = DeviceIoControl(&codadev_api, 8, mountstring, strlen(mountstring), NULL, 0);
  if (res) {
    printf ("Unmount failed: %d\n", res);
    //   exit (1);
  }

  printf ("Unmount OK\n");

  do {
    printf ("UNLOAD result %d\n", res = unload_vxd("CODADEV "));    
  } while (res == 0);

  /*  out.oh.opcode = 99;
  addr.sin_addr.s_addr = htonl (0x7f000001);
  addr.sin_port = htons(8001); 
  res = sendto(udpfd, (char *) &out, sizeof(out), 0,
	       (struct sockaddr *) &addr, sizeof(addr));
  if (res == -1)
  perror ("sendto");*/
}


