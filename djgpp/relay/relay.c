
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <stdio.h>
#include <errno.h>

#include <cfs/coda.h>

#define BUFSIZE 2000

struct optab {
  int opcode;
  char *name;
} ops[] = {
  {CFS_ROOT, "CFS_ROOT"},
  {CFS_SYNC, "CFS_SYNC"},
  {CFS_OPEN, "CFS_OPEN"},
  {CFS_CLOSE, "CFS_CLOSE"},
  {CFS_IOCTL, "CFS_IOCTL"},
  {CFS_GETATTR, "CFS_GETATTR"},
  {CFS_SETATTR, "CFS_SETATTR"},
  {CFS_ACCESS, "CFS_ACCESS"},
  {CFS_LOOKUP, "CFS_LOOKUP"},
  {CFS_CREATE, "CFS_CREATE"},
  {CFS_REMOVE, "CFS_REMOVE"},
  {CFS_LINK, "CFS_LINK"},
  {CFS_RENAME, "CFS_RENAME"},
  {CFS_MKDIR, "CFS_MKDIR"},
  {CFS_RMDIR, "CFS_RMDIR"},
  {CFS_READDIR, "CFS_READDIR"},
  {CFS_SYMLINK, "CFS_SYMLINK"},
  {CFS_READLINK, "CFS_READLINK"},
  {CFS_FSYNC, "CFS_FSYNC"},
  {CFS_INACTIVE, "CFS_INACTIVE"},
  {CFS_VGET, "CFS_VGET"},
  {CFS_SIGNAL, "CFS_SIGNAL"},
  {CFS_REPLACE, "CFS_REPLACE"},
  {CFS_FLUSH, "CFS_FLUSH"},
  {CFS_PURGEUSER, "CFS_PURGEUSER"},
  {CFS_ZAPFILE, "CFS_ZAPFILE"},
  {CFS_ZAPDIR, "CFS_ZAPDIR"},
  {CFS_ZAPVNODE, "CFS_ZAPVNODE"},
  {CFS_PURGEFID, "CFS_PURGEFID"},
  {CFS_OPEN_BY_PATH, "CFS_OPEN_BY_PATH"},
  {99, "<<CLOSE>>"},
  {-99, NULL}};


char *
opcode(int op)
{
  struct optab *p;
  static char buf[100];

  for (p = ops; p->opcode != -99; p++) {
    if (p->opcode == op)
      return p->name;
  }
  sprintf (buf, "[opcode %d]", op);
  return buf;
}

void
printvfid(ViceFid *vfid)
{
  printf ("<%lx,%ld,%ld> ", vfid->Volume, vfid->Vnode, vfid->Unique);
}

void
printattr(struct coda_vattr *p)
{
  printf ("mode %x nlink %d uid %d gid %d size %ld ",
	  p->va_mode, p->va_nlink, p->va_uid, p->va_gid,
	  p->va_size);
}

void
printrequest (char *buffer)
{
  union inputArgs *in = (union inputArgs *) buffer;

  printf ("req: %s uniq %d ", opcode(in->ih.opcode), in->ih.unique);
  switch (in->ih.opcode) {
  case CFS_OPEN:
    printvfid (&in->cfs_open.VFid);
    printf ("(fl %d) ", in->cfs_open.flags);
    break;
  case CFS_OPEN_BY_PATH:
    printvfid (&in->cfs_open_by_path.VFid);
    printf ("(fl %d) ", in->cfs_open.flags);
    break;
  case CFS_LOOKUP:
    printvfid (&in->cfs_lookup.VFid);
    printf ("\"%s\"", buffer + (int) in->cfs_lookup.name);
    break;
  case CFS_CREATE:
    printvfid (&in->cfs_create.VFid);
    printf ("\"%s\" ", buffer + (int) in->cfs_create.name);
    printf ("mode %d", in->cfs_create.mode);
    break;
  case CFS_CLOSE:
    printvfid (&in->cfs_close.VFid);
    printf ("(fl %d) ", in->cfs_close.flags);
    break;
  }
  printf ("\n");
}

void
printreply (char *buffer, int n, struct sockaddr_in *addr)
{
  union outputArgs *out = (union outputArgs *) buffer;

  /*
  printf ("[%08x:%d -- %d bytes] ", ntohl(addr->sin_addr.s_addr), 
	  ntohs(addr->sin_port), n);
  */
  if (n) {
    printf ("ans: %s uniq %d ", opcode(out->oh.opcode), out->oh.unique);
    if (out->oh.opcode >= CFS_REPLACE && out->oh.opcode <= CFS_PURGEFID)
      printf (" (DOWNCALL) ");
    else {
      if (out->oh.result)
	printf (" res %d (%s) ", out->oh.result, 
		(out->oh.result >= 1 && out->oh.result <= sys_nerr) ?
		sys_errlist[out->oh.result] : "?");
      else
	printf (" (success) ");
    }
    switch (out->oh.opcode) {
    case CFS_ROOT:
      printvfid(&out->cfs_root.VFid);
      break;

    case CFS_GETATTR:
      printattr(&out->cfs_getattr.attr);
      break;
	  
    case CFS_LOOKUP:
      printvfid(&out->cfs_lookup.VFid);
      printf ("type %d", out->cfs_lookup.vtype);
      break;

    case CFS_OPEN_BY_PATH:
      printf (" path %s", (char *) out + (int) out->cfs_open_by_path.path);
      break;

    case CFS_CREATE:
      printvfid(&out->cfs_create.VFid);
      printattr(&out->cfs_create.attr);
      break;

    case CFS_READDIR:
      buffer[(int) out->cfs_readdir.data + out->cfs_readdir.size] = '\0';
      printf ("size %d buf %s", out->cfs_readdir.size, 
	      buffer + (int) out->cfs_readdir.data);
      break;
    }
  }
  printf ("\n");
}


struct far_ptr {
  unsigned int offset; 
  unsigned short segment;
};
struct far_ptr mc_api = {0,0};

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

  if (mc_api.offset == 0)
    return -1;

  asm ("lcall  %1"
       : "=a" (err)
       : "m" (mc_api), "a" (func), "S" (inBuf), "b" (inCount),
       "D" (outBuf), "c" (outCount), "d" (123456));
  if (err)
    printf ("DeviceIoControl %d: err %d\n", func, err);
  return err;
}


extern int __quiet_socket;

void send_buf (int udpfd, int flag, char *buf, int n)
{
  struct sockaddr_in addr;
  char buffer[BUFSIZE + 4];

  *(int *)buffer = flag;
  memcpy (buffer + 4, buf, n);

  addr.sin_addr.s_addr = ntohl(0x7f000001);
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
  union outputArgs *out = (union outputArgs *)buffer;

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

  if (open_vxd("MC      ", &mc_api)) 
    printf ("MC.VXD already loaded\n");
  else {
    int version;
    if (!open_vxdldr())
      exit (1);
    if (!vxdldr_get_version(&version)) {
      printf ("cannot get VXDLDR version\n");
      exit (1);
    }
    printf ("VXDLDR version %x\n", version);
    if ((res = vxdldr_load_device("mc.vxd"))) {
      printf ("cannot load MC: VXDLDR error %d\n", res);
      exit (1);
    }
    if (!open_vxd("MC      ", &mc_api)) {
      printf ("Loaded MC, but could not get api\n");
      exit (1);
    }
    printf ("Loaded MC.VXD\n");
  }

  {
    unsigned int calls[20];
    int i;

    res = DeviceIoControl(1, NULL, 0, calls, sizeof(calls));
    if (res) {
      printf ("DeviceIoControl 1 failed: %d\n", res);
      exit(1);
    }
    printf ("MC net ID %x, provider ID %x\n", calls[0], calls[1]);
  }

  mcfd = 10;
  err = DeviceIoControl(9, &mcfd, sizeof(mcfd), NULL, 0);
  if (err) {
    printf ("Error in allocateFD: %d\n", err);
    exit (1);
  }
  __addselectablefd (mcfd);

  timeo = 0;
  while (1) {
    FD_ZERO (&rfd);
    FD_SET (udpfd, &rfd);
    FD_SET (mcfd, &rfd);

    tv.tv_sec = 30;
    tv.tv_usec = 0;
    fflush(stdout);
    nfd = select(32, &rfd, NULL, NULL, &tv);
    if (nfd < 0) {
      perror ("select");
      exit (1);
    } else if (nfd == 0) {
      printf ("time ");
      fflush(stdout);
      timeo++;
      continue;
    }
    if (timeo)
      printf ("\n");
    timeo = 0;

    if (FD_ISSET (udpfd, &rfd)) {
      fromlen = sizeof(addr);
      n = recvfrom (udpfd, buffer, BUFSIZE, 0, 
		    (struct sockaddr *)&addr, &fromlen);
      if (n == -1) {
	perror ("recvfrom");
	goto mc;
      }

      send_buf (udpfd, 1, buffer, n);

      if (out->oh.opcode == 99) {
	printf (" -------- CLOSE -------- \n");
	goto done;
      }
      printreply (buffer, n, &addr);
      err = DeviceIoControl (4, buffer, n, NULL, 0);
      if (err)
	printf ("SENDREPLY error %d\n", err);
    }

  mc:
    if (FD_ISSET (mcfd, &rfd)) {
      wait = 0;
      err = DeviceIoControl (3, &wait, sizeof(wait), buffer, BUFSIZE);
      if (err == 38) {
	printf ("No request available!\n");
	continue;
      } else if (err) {
	printf ("GETREQUEST error %d\n", err);
	continue;
      }
      send_buf (udpfd, 0, buffer, BUFSIZE);
      printrequest (buffer);
      addr.sin_addr.s_addr = htonl(0x7f000001);
      addr.sin_port = htons(8000);
      res = sendto(udpfd, buffer, BUFSIZE, 0, (struct sockaddr *) &addr,
		   sizeof(addr));
      if (res == -1)
	perror ("sendto");
    }
  }
done:
  mcfd = 10;
  err = DeviceIoControl(10, &mcfd, sizeof(mcfd), NULL, 0);
  if (err) {
    printf ("Error in deallocateFD: %d\n", err);
    printf ("will not unload MC\n");
    exit (1);
  }
  do {
    printf ("UNLOAD result %d\n", res = vxdldr_unload_device("MC"));
  } while (res == 0);
}


