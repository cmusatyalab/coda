
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <string.h>
#include <ctype.h>
#include <cfs/coda.h>

#define BUFSIZE 2000
#define INSIZE(tag) sizeof(struct cfs_ ## tag ## _in)
#define OUTSIZE(tag) sizeof(struct cfs_ ## tag ## _out)
#define SIZE(tag)  max(INSIZE(tag), OUTSIZE(tag))

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
  {CFS_PURGEFID, "CFS_PURGEFID"},
  {-1, NULL}};


int
get_opcode(char *buf)
{
  struct optab *p;
  char *q;

  for (q = buf; *q; q++)
    *q = toupper(*q);

  for (p = ops; p->opcode != -99; p++)
    if (!strncmp(buf, p->name, strlen(p->name)))
      break;
  return p->opcode;
}

void
printvfid(ViceFid *vfid)
{
  printf ("<%lx,%ld,%ld>", vfid->Volume, vfid->Vnode, vfid->Unique);
}

int
getvfid(ViceFid *vfid)
{
  char buf[100];
  printf ("VFid: volume 0x7f000000, Vnode, Unique: ");
  fflush(stdout);
  if (!gets(buf))
    return -1;

  if (sscanf (buf, "%ld %ld", &vfid->Vnode, &vfid->Unique) != 2)
    return -1;
  vfid->Volume = 0x7f000000;
  return 0;
}


main()
{
  int fd, nfd, n;
  struct sockaddr_in addr;
  char buf[1000];
  union inputArgs *in = (union inputArgs *)buf;
  char reply[100];
  int unique = 1000;

  fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (fd == -1) {
    perror ("socket");
    exit (1);
  }

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = 0;
  if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
    perror ("bind");
    exit (1);
  }

  addr.sin_addr.s_addr = htonl(0xc0a80103);
  addr.sin_port = htons(8000);

  while (1) {
  again:
    printf ("OPCODE: ");
    fflush(stdout);
    if(!gets (reply)) {
      printf ("\ngood-bye\n");
      exit(0);
    }

    in->cfs_getattr.ih.opcode = get_opcode(reply);
    in->cfs_getattr.ih.unique = unique++;
    in->cfs_getattr.ih.pid = 200;
    in->cfs_getattr.ih.pgid = 200;
    memset(&in->cfs_getattr.ih.cred, 0, sizeof(struct coda_cred));

    if (in->ih.opcode == -1) {
      printf ("%s not an opcode\n");
      continue;
    }
    
    switch (in->cfs_getattr.ih.opcode) {
    case CFS_ROOT:
      n = sizeof(*in);
      break;

    case CFS_GETATTR:
      n = INSIZE(getattr);
      if (getvfid(&in->cfs_getattr.VFid) == -1)
	goto again;
      break;

    case CFS_LOOKUP:
      n = INSIZE(lookup);
      printf ("Parent VFid: ");
      if (getvfid(&in->cfs_lookup.VFid) == -1)
	goto again;
      printf ("Name: ");
      if (!gets(&buf[n]))
	goto again;
      in->cfs_lookup.name = n;
      n += strlen(&buf[n]) + 1;
      break;

    case CFS_OPEN:
      n = sizeof(struct inputArgs);
      if (getvfid(&in->cfs_open.VFid) == -1)
	goto again;
      printf ("Flags: ");
      if (!gets(reply))
	goto again;
      in->cfs_open.flags = atoi(reply);
      break;

    case CFS_CLOSE:
      n = sizeof(struct inputArgs);
      if (getvfid(&in->cfs_close.VFid) == -1)
	goto again;
      printf ("Flags: ");
      if (!gets(reply))
	goto again;
      in->cfs_close.flags = atoi(reply);
      break;

    case CFS_CREATE:
      n = sizeof(struct inputArgs);
      printf ("Parent: ");
      if (getvfid(&in->cfs_create.VFid) == -1)
	goto again;
      in->cfs_create.attr.va_mode = 0666;
      in->cfs_create.mode = 0666;
      in->cfs_create.name = n;
      printf ("Name: ");
      if (!gets(&buf[n]))
	goto again;
      n += strlen(&buf[n]) + 1;
      break;

    case CFS_READDIR:
      n = sizeof(struct inputArgs);
      if (getvfid(&in->cfs_readdir.VFid) == -1)
	goto again;
      in->cfs_readdir.count = 50;
      in->cfs_readdir.offset = 0;
      break;

    default:
      printf ("%s not supported\n", reply);
      goto again;
    }

    n = sendto (fd, buf, n, 0, 
		(struct sockaddr *)&addr, sizeof(addr));
    if (n == -1) {
      perror ("sendto");
      exit (1);
    }
  }
}

