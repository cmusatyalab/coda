
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <stdio.h>
#include <errno.h>

#include <cfs/coda.h>

#define BUFSIZE 2000

FILE *file;

struct optab {
  int opcode;
  char *name;
} ops[] = {
  {CODA_ROOT, "CODA_ROOT"},
  {CODA_SYNC, "CODA_SYNC"},
  {CODA_OPEN, "CODA_OPEN"},
  {CODA_CLOSE, "CODA_CLOSE"},
  {CODA_IOCTL, "CODA_IOCTL"},
  {CODA_GETATTR, "CODA_GETATTR"},
  {CODA_SETATTR, "CODA_SETATTR"},
  {CODA_ACCESS, "CODA_ACCESS"},
  {CODA_LOOKUP, "CODA_LOOKUP"},
  {CODA_CREATE, "CODA_CREATE"},
  {CODA_REMOVE, "CODA_REMOVE"},
  {CODA_LINK, "CODA_LINK"},
  {CODA_RENAME, "CODA_RENAME"},
  {CODA_MKDIR, "CODA_MKDIR"},
  {CODA_RMDIR, "CODA_RMDIR"},
  {CODA_READDIR, "CODA_READDIR"},
  {CODA_SYMLINK, "CODA_SYMLINK"},
  {CODA_READLINK, "CODA_READLINK"},
  {CODA_FSYNC, "CODA_FSYNC"},
  {CODA_INACTIVE, "CODA_INACTIVE"},
  {CODA_VGET, "CODA_VGET"},
  {CODA_SIGNAL, "CODA_SIGNAL"},
  {CODA_REPLACE, "CODA_REPLACE"},
  {CODA_FLUSH, "CODA_FLUSH"},
  {CODA_PURGEUSER, "CODA_PURGEUSER"},
  {CODA_ZAPFILE, "CODA_ZAPFILE"},
  {CODA_ZAPDIR, "CODA_ZAPDIR"},
  {CODA_PURGEFID, "CODA_PURGEFID"},
  {CODA_OPEN_BY_PATH, "CODA_OPEN_BY_PATH"},
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
  fprintf (file, "<%lx,%ld,%ld> ", vfid->Volume, vfid->Vnode, vfid->Unique);
}

void
printattr(struct coda_vattr *p)
{
  fprintf (file, "mode %x nlink %d uid %d gid %d size %ld ",
	  p->va_mode, p->va_nlink, p->va_uid, p->va_gid,
	  p->va_size);
}

void
printrequest (char *buffer)
{
  union inputArgs *in = (union inputArgs *) buffer;

  fprintf (file, "req: %s uniq %d ", opcode(in->ih.opcode), in->ih.unique);
  switch (in->ih.opcode) {
  case CODA_OPEN:
    printvfid (&in->coda_open.VFid);
    fprintf (file, "(fl %d) ", in->coda_open.flags);
    break;
  case CODA_OPEN_BY_PATH:
    printvfid (&in->coda_open_by_path.VFid);
    fprintf (file, "(fl %d) ", in->coda_open.flags);
    break;
  case CODA_LOOKUP:
    printvfid (&in->coda_lookup.VFid);
    fprintf (file, "\"%s\"", buffer + (int) in->coda_lookup.name);
    break;
  case CODA_CREATE:
    printvfid (&in->coda_create.VFid);
    fprintf (file, "\"%s\" ", buffer + (int) in->coda_create.name);
    fprintf (file, "mode %d", in->coda_create.mode);
    break;
  case CODA_MKDIR:
    printvfid (&in->coda_mkdir.VFid);
    fprintf (file, "\"%s\" ", buffer + (int) in->coda_mkdir.name);
    break;
  case CODA_CLOSE:
    printvfid (&in->coda_close.VFid);
    fprintf (file, "(fl %d) ", in->coda_close.flags);
    break;
  case CODA_REMOVE:
    printvfid (&in->coda_remove.VFid);
    fprintf (file, "\"%s\" ", buffer + (int) in->coda_remove.name);
    break;
  }
  fprintf (file, "\n");
}

void
printreply (char *buffer, int n, struct sockaddr_in *addr)
{
  union outputArgs *out = (union outputArgs *) buffer;

  /*
  fprintf (file, "[%08x:%d -- %d bytes] ", ntohl(addr->sin_addr.s_addr), 
	  ntohs(addr->sin_port), n);
  */
  if (n) {
    fprintf (file, "ans: %s uniq %d ", opcode(out->oh.opcode), out->oh.unique);
    if (out->oh.opcode >= CODA_REPLACE && out->oh.opcode <= CODA_PURGEFID)
      fprintf (file, " (DOWNCALL) ");

    if (out->oh.result!=0)
      fprintf (file, " venus returned res %d (%s) ", out->oh.result, 
		(out->oh.result >= 1 && out->oh.result <= sys_nerr) ?
		sys_errlist[out->oh.result] : "?");
    else {
      fprintf (file, " (success) ");

      //These can only be printed if no error was returned by vice
      switch (out->oh.opcode) {
      case CODA_ROOT:
        printvfid(&out->coda_root.VFid);
        break;
      case CODA_LOOKUP:
        printvfid(&out->coda_lookup.VFid);
        fprintf (file, "type %d", out->coda_lookup.vtype);
        break;

      case CODA_OPEN_BY_PATH:
   	fprintf (file, " path %s", (char *) out + (int) out->coda_open_by_path.path);        
        break;

      case CODA_CREATE:
	printvfid(&out->coda_create.VFid);
	printattr(&out->coda_create.attr);
	break;

      case CODA_MKDIR:
	printvfid(&out->coda_mkdir.VFid);
	printattr(&out->coda_mkdir.attr);
	break;
    
    }//switch
    }//else

    //These can even be printed when vice returned an error
    switch (out->oh.opcode) {
   
    case CODA_GETATTR:
      printattr(&out->coda_getattr.attr);
      break;
	    
    case CODA_READDIR:
      buffer[(int) out->coda_readdir.data + out->coda_readdir.size] = '\0';
      fprintf (file, "size %d buf %s", out->coda_readdir.size, 
      	buffer + (int) out->coda_readdir.data);
      break;
    }//switch

  }//if (n)

  fprintf (file, "\n");
}


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
    fprintf (file, "DeviceIoControl %d: err %d\n", func, err);
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

  file = fopen ("relay.log", "w+");
    if (!file){
     perror("file");
     exit(1);
  }

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

  if (open_vxd("CODADEV ", &codadev_api)) 
    fprintf (file, "CODADEV.VXD already loaded\n");
  else {
    int version;
    if (!open_vxdldr())
      exit (1);
    if (!vxdldr_get_version(&version)) {
      fprintf (file, "cannot get VXDLDR version\n");
      exit (1);
    }
    fprintf (file, "VXDLDR version %x\n", version);
    if ((res = vxdldr_load_device("codadev.vxd"))) {
      fprintf (file, "cannot load CODADEV: VXDLDR error %d\n", res);
      exit (1);
    }
    if (!open_vxd("CODADEV ", &codadev_api)) {
      fprintf (file, "Loaded CODADEV, but could not get api\n");
      exit (1);
    }
    fprintf (file, "Loaded CODADEV.VXD\n");
  }

  {
    unsigned int calls[20];
    int i;

    res = DeviceIoControl(1, NULL, 0, calls, sizeof(calls));
    if (res) {
      fprintf (file, "DeviceIoControl 1 failed: %d\n", res);
      exit(1);
    }
    fprintf (file, "CODADEV net ID %x, provider ID %x\n", calls[0], calls[1]);
  }

  mcfd = 10;
  err = DeviceIoControl(9, &mcfd, sizeof(mcfd), NULL, 0);
  if (err) {
    fprintf (file, "Error in allocateFD: %d\n", err);
    exit (1);
  }
  __addselectablefd (mcfd);

  fflush(file);

  timeo = 0;
  while (1) {
    FD_ZERO (&rfd);
    FD_SET (udpfd, &rfd);
    FD_SET (mcfd, &rfd);

    tv.tv_sec = 30;
    tv.tv_usec = 0;
    fflush(stdout);fflush(file);
    nfd = select(32, &rfd, NULL, NULL, &tv);
    if (nfd < 0) {
      perror ("select");
      exit (1);
    } else if (nfd == 0) {
      fprintf (file, "time ");
      fflush(stdout);fflush(file);
      timeo++;
      continue;
    }
    if (timeo)
      fprintf (file, "\n");
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
	fprintf (file, " -------- CLOSE -------- \n");
	goto done;
      }
      printreply (buffer, n, &addr);
      err = DeviceIoControl (4, buffer, n, NULL, 0);
      if (err)
	fprintf (file, "SENDREPLY error %d\n", err);
    }

  mc:
    if (FD_ISSET (mcfd, &rfd)) {
      wait = 0;
      err = DeviceIoControl (3, &wait, sizeof(wait), buffer, BUFSIZE);
      if (err == 38) {
	fprintf (file, "No request available!\n");
	continue;
      } else if (err) {
	fprintf (file, "GETREQUEST error %d\n", err);
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
  fclose(file);
  mcfd = 10;
  err = DeviceIoControl(10, &mcfd, sizeof(mcfd), NULL, 0);
  if (err) {
    printf ("Error in deallocateFD: %d\n", err);
    printf ("will not unload CODADEV\n");
    exit (1);
  }
  do {
    printf ("UNLOAD result %d\n", res = vxdldr_unload_device("CODADEV"));
  } while (res == 0);

}


