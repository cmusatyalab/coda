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
#include "relay.h"


//#include <auth2.h>

//typedef struct {
  //  int			    sTokenSize;
  //  EncryptedSecretToken    stoken;
  //  int			    cTokenSize;
  //  ClearToken		    ctoken;
  //  } venusbuff;

#define BUFSIZE 4096

static int mcfd = 16;
int inited = 0;
FILE *file;
int udpfd;  
struct sockaddr_in addr;
struct far_ptr codadev_api = {0,0};

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
 // venusbuff *vb ; 
  struct timeval t;
  gettimeofday(&t, 0);
  fprintf (file, "req: %s uniq %d sec %i usec %i", opcode(in->ih.opcode), in->ih.unique, t.tv_sec, t.tv_usec);
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
  case CODA_IOCTL:
    printvfid(&in->coda_ioctl.VFid);    
 //   vb = (venusbuff *)(buffer + (int )in->coda_ioctl.data);
 //   fprintf (file, "vb \"%i\" ", vb->ctoken.EndTimestamp); 
    break;
  }
  fprintf (file, "\n");
}

void
printreply (char *buffer, int n, struct sockaddr_in *addr)
{
  union outputArgs *out = (union outputArgs *) buffer;
  struct timeval t;
  gettimeofday(&t, 0);
  fprintf(file, "sec %i usec %i\n", t.tv_sec, t.tv_usec);
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

      /* These can only be printed if no error was returned by vice */
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
    
      }/*switch*/
    }/*else*/

    /*These can even be printed when vice returned an error*/
    switch (out->oh.opcode) {
   
    case CODA_GETATTR:
      printattr(&out->coda_getattr.attr);
      break;
	    
    case CODA_READDIR:
      buffer[(int) out->coda_readdir.data + out->coda_readdir.size] = '\0';
      fprintf (file, "size %d buf %s", out->coda_readdir.size, 
      	buffer + (int) out->coda_readdir.data);
      break;
    }/*switch*/

  }/*if (n)*/

  fprintf (file, "\n");
}

void send_buf (int udpfd, int flag, char *buf, int n)
{
  struct sockaddr_in addr;
  char buffer[BUFSIZE + 4];

  *(int *)buffer = flag;
  memcpy (buffer + 4, buf, n);

  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = ntohs(9000);
  sendto (udpfd, buffer, n+4, 0, (struct sockaddr *) &addr, sizeof(addr));
}

int close_relay()
{
  int err;
  err = DeviceIoControl(&codadev_api, 10, &mcfd, sizeof(mcfd), NULL, 0);
  if (err) {
    printf ("Error in deallocateFD: %d\n", err);
    printf ("will not unload CODADEV\n");
    return 0;
  }
  fclose(file);
  inited = 0;
  return 1;
}

int init_relay()
{
  int err, res;
  inited = 0;

  file = fopen ("relay.log", "w+");
    if (!file){
     perror("file");
     return 0;
  }  
  __djgpp_set_quiet_socket(1);

  udpfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (udpfd == -1) {
    fprintf(file, "socket\n");
    return 0;
  }

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(8001);
  if (bind(udpfd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
    fprintf(file, "bind\n");
    return 0;;
  }  

  if (open_vxd("CODADEV ", &codadev_api)) 
    fprintf (file, "CODADEV.VXD already loaded\n");
  else {
    if (load_vxd("CODADEV ", "codadev.vxd", &codadev_api))	  
	    fprintf (file, "Loaded CODADEV.VXD\n");
    else return 0;
  }

  {
    unsigned int calls[20];
    int i;

    res = DeviceIoControl(&codadev_api, 1, NULL, 0, calls, sizeof(calls));
    if (res) {
      fprintf (file, "DeviceIoControl 1 failed: %d\n", res);
      return 0;
    }
    fprintf (file, "CODADEV net ID %x, provider ID %x\n", calls[0], calls[1]);
  }

  err = DeviceIoControl(&codadev_api, 9, &mcfd, sizeof(mcfd), NULL, 0);
  if (err) {
    fprintf (file, "Error in allocateFD: %d\n", err);
    return 0;
  }
  __addselectablefd (mcfd);

  inited = 1;
  return 1;
}

int mount_relay(char *mountpoint)
{
    char mountstring[] = "Z\\\\CODA\\CLUSTER";
    int res;

    fprintf (file, "Mounting on %s\n", mountpoint);

    mountstring[0] = toupper(mountpoint[0]);

    res = DeviceIoControl(&codadev_api, 2, mountstring, strlen(mountstring), NULL, 0);
    if (res) {
	fprintf (file, "Mount failed: %d\n", res);
	return 0;
    }

    fprintf (file, "Mount OK\n");
    return 1;
}

int unmount_relay()
{
    int res;	
    res = DeviceIoControl(&codadev_api, 8, NULL, 0, NULL, 0);
    if (res) {
	    fprintf (file, "Unmount failed: %d\n", res);
	    return 0;
    }
    fprintf (file, "Unmount OK\n");
    return 1;
}

int read_relay(char *buffer)
{
  int err;	
  int wait = 0;

  if (inited){
    err = DeviceIoControl (&codadev_api, 3, &wait, sizeof(wait), buffer, BUFSIZE);
    if (err == 38) {
      fprintf (file, "No request available!\n");
      return -1;
    } else if (err) {
      fprintf (file, "GETREQUEST error %d\n", err);
      return -1;
    }
    send_buf (udpfd, 0, buffer, BUFSIZE);
    printrequest (buffer);
  }
  return BUFSIZE;
}
int write_relay(char *buffer, int n)
{ 
  int err;
  
  if(inited){
    send_buf (udpfd, 1, buffer, n);
    printreply (buffer, n, &addr);
    err = DeviceIoControl (&codadev_api, 4, buffer, n, NULL, 0);
    if (err){
      fprintf (file, "SENDREPLY error %d\n", err);
      return n;
    }
  }
  return n;
}


#ifdef 0
/* Relay used to be a seperate program. below is the old main function 
   for reference */
main()
{
  int nfd, n;
  int err, res, wait;
  fd_set rfd;
  struct timeval tv;
  int fromlen, timeo;
  char buffer[BUFSIZE];
  union outputArgs *out = (union outputArgs *)buffer;

  if (!init_relay())
      exit(1);

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
      err = DeviceIoControl (&codadev_api, 4, buffer, n, NULL, 0);
      if (err)
	fprintf (file, "SENDREPLY error %d\n", err);

    }

  mc:
    if (FD_ISSET (mcfd, &rfd)) {
      wait = 0;
      err = DeviceIoControl (&codadev_api, 3, &wait, sizeof(wait), buffer, BUFSIZE);
      if (err == 38) {
	fprintf (file, "No request available!\n");
	continue;
      } else if (err) {
	fprintf (file, "GETREQUEST error %d\n", err);
	continue;
      }
      send_buf (udpfd, 0, buffer, BUFSIZE);
      printrequest (buffer);
      addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      addr.sin_port = htons(8000);
      res = sendto(udpfd, buffer, BUFSIZE, 0, (struct sockaddr *) &addr,
		   sizeof(addr));
      if (res == -1)
	perror ("sendto");
    }
  }
done:
  if (!close_relay())
    exit(1);

  do {
    printf ("UNLOAD result %d\n", res = unload_vxd("CODADEV"));    
  } while (res == 0);
  res = 0;
  do {
    printf ("UNLOAD result %d\n", res = unload_vxd("SOCK"));    
  } while (res == 0);
}


#endif
