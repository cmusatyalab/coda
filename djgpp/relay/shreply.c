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

typedef u_long VolumeId;
typedef u_long VnodeId;
typedef u_long Unique;
typedef struct ViceFid {
    VolumeId Volume;
    VnodeId Vnode;
    Unique Unique;
} ViceFid;

#include <cfs/cfs.h>

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
  {CFS_RDWR, "CFS_RDWR"},
  {ODY_MOUNT, "ODY_MOUNT"},
  {ODY_LOOKUP, "ODY_LOOKUP"},
  {ODY_EXPAND, "ODY_EXPAND"},
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
printattr(struct vattr *p)
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
  struct outputArgs *out = (struct outputArgs *) buffer;

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
      printf ("%s uniq %d res %d ", opcode(out->opcode), 
	      out->unique, out->result);
      switch (out->opcode) {
      case CFS_ROOT:
	printvfid(&out->d.cfs_root.VFid);
	break;

      case CFS_GETATTR:
	printattr(&out->d.cfs_getattr.attr);
	break;

      case CFS_LOOKUP:
	printvfid(&out->d.cfs_lookup.VFid);
	printf (" type %d", out->d.cfs_lookup.vtype);
	break;

      case CFS_OPEN:
	printf (" dev %d inode %d", out->d.cfs_open.dev, 
		out->d.cfs_open.inode);
	break;

      case CFS_CREATE:
	printvfid(&out->d.cfs_create.VFid);
	printattr(&out->d.cfs_create.attr);
	break;

      case CFS_READDIR:
	buffer[(int) out->d.cfs_readdir.data + out->d.cfs_readdir.size] = '\0';
	printf ("size %d buf %s", out->d.cfs_readdir.size, 
		buffer + (int) out->d.cfs_readdir.data);
	break;
      }
    }
    printf ("\n");
  }
}

