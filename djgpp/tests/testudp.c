
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <stdio.h>
#include <errno.h>

#define BUFSIZE 1000

extern int __quiet_socket;

void send_buf (int udpfd, int flag, char *buf, int n)
{
  struct sockaddr_in addr;
  char buffer[BUFSIZE + 4];

  *(int *)buffer = flag;
  memcpy (buffer + 4, buf, n);

  addr.sin_addr.s_addr = ntohl(0x8002DED2);
  addr.sin_port = ntohs(9000);
  sendto (udpfd, buffer, n+4, 0, (struct sockaddr *) &addr, sizeof(addr));
}

main()
{
  int udpfd, mcfd, nfd, n;
  int err, res, wait;
  fd_set rfd;
  struct sockaddr_in addr;
  struct timeval tv;
  int fromlen, timeo;
  char buffer[BUFSIZE];
  struct outputArgs *out = (struct outputArgs *)buffer;

  __quiet_socket = 1;

  udpfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (udpfd == -1) {
    perror ("socket");
    exit (1);
  }

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(8001);
  if (bind(udpfd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
    perror ("bind");
    exit (1);
  }

/*
      n = recvfrom (udpfd, buffer, BUFSIZE, 0, 
		    (struct sockaddr *)&addr, &fromlen);
      if (n == -1) {
	perror ("recvfrom");
	goto mc;
      }
*/

  send_buf (udpfd, 1, buffer, 8);

}


