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


#include <coda.h>

#define BUFSIZE 2000

struct optab {
  int opcode;
  char *name;
} ops[] = {
  {CODA_ROOT, "CODA_ROOT"},
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
  {CODA_SYMLINK, "CODA_SYMLINK"},
  {CODA_READLINK, "CODA_READLINK"},
  {CODA_FSYNC, "CODA_FSYNC"},
  {CODA_VGET, "CODA_VGET"},
  {CODA_SIGNAL, "CODA_SIGNAL"},
  {CODA_REPLACE, "CODA_REPLACE"},
  {CODA_FLUSH, "CODA_FLUSH"},
  {CODA_PURGEUSER, "CODA_PURGEUSER"},
  {CODA_ZAPFILE, "CODA_ZAPFILE"},
  {CODA_ZAPDIR, "CODA_ZAPDIR"},
  {CODA_PURGEFID, "CODA_PURGEFID"},
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
  printf ("<%lx,%ld,%ld>", vfid->Volume, vfid->Vnode, vfid->Unique);
}

void
printattr(struct coda_vattr *p)
{
  printf ("mode %x nlink %d uid %d gid %d size %ld ",
	  p->va_mode, p->va_nlink, p->va_uid, p->va_gid,
	  p->va_size);
}

main()
{
  int fd, nfd, n;
  struct sockaddr_in addr;
  int fromlen;
  char buffer[BUFSIZE];
  union outputArgs *out = (struct outputArgs *) buffer;

  fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (fd == -1) {
    perror ("socket");
    exit (1);
  }

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(8001);
  if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
    perror ("bind");
    exit (1);
  }

  while (1) {
    fromlen = sizeof(addr);
    n = recvfrom (fd, buffer, BUFSIZE, 0, (struct sockaddr *)&addr, &fromlen);
    if (n == -1) {
      perror ("recvfrom");
      exit (1);
    }
    printf ("[%08x:%d -- %d bytes] ", ntohl(addr.sin_addr.s_addr), 
	    ntohs(addr.sin_port), n);
    if (n) {
      printf ("%s uniq %d res %d ", opcode(out->coda_getattr.oh.opcode), 
	      out->coda_getattr.oh.unique, out->coda_getattr.oh.result);
      switch (out->coda_getattr.oh.opcode) {
      case CODA_ROOT:
	printvfid(&out->coda_root.VFid);
	break;

      case CODA_GETATTR:
	printattr(&out->coda_getattr.attr);
	break;

      case CODA_LOOKUP:
	printvfid(&out->coda_lookup.VFid);
	printf (" type %d", out->coda_lookup.vtype);
	break;

      case CODA_OPEN:
	printf (" dev %d inode %d", out->coda_open.dev, 
		out->coda_open.inode);
	break;

      case CODA_CREATE:
	printvfid(&out->coda_create.VFid);
	printattr(&out->coda_create.attr);
	break;

#if 0 /* no longer used */
      case CODA_READDIR:
	buffer[(int) out->coda_readdir.data + out->coda_readdir.size] = '\0';
	printf ("size %d buf %s", out->coda_readdir.size, 
		buffer + (int) out->coda_readdir.data);
	break;
#endif
      }
    }
    printf ("\n");
  }
}

