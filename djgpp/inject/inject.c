/*
   Copyright 1997-98 Michael Callahan
   This program is free software.  You may copy it according
   to the conditions of the GNU General Public License version 2.0
   or later; see the file COPYING in the source directory.
*/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <string.h>
#include <ctype.h>
#include <coda.h>

#define BUFSIZE 2000
#define INSIZE(tag) sizeof(struct coda_ ## tag ## _in)
#define OUTSIZE(tag) sizeof(struct coda_ ## tag ## _out)
#define SIZE(tag)  max(INSIZE(tag), OUTSIZE(tag))

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

    in->coda_getattr.ih.opcode = get_opcode(reply);
    in->coda_getattr.ih.unique = unique++;
    in->coda_getattr.ih.pid = 200;
    in->coda_getattr.ih.pgid = 200;
    memset(&in->coda_getattr.ih.cred, 0, sizeof(struct coda_cred));

    if (in->ih.opcode == -1) {
      printf ("%s not an opcode\n");
      continue;
    }
    
    switch (in->coda_getattr.ih.opcode) {
    case CODA_ROOT:
      n = sizeof(*in);
      break;

    case CODA_GETATTR:
      n = INSIZE(getattr);
      if (getvfid(&in->coda_getattr.VFid) == -1)
	goto again;
      break;

    case CODA_LOOKUP:
      n = INSIZE(lookup);
      printf ("Parent VFid: ");
      if (getvfid(&in->coda_lookup.VFid) == -1)
	goto again;
      printf ("Name: ");
      if (!gets(&buf[n]))
	goto again;
      in->coda_lookup.name = n;
      n += strlen(&buf[n]) + 1;
      break;

    case CODA_OPEN:
      n = sizeof(struct inputArgs);
      if (getvfid(&in->coda_open.VFid) == -1)
	goto again;
      printf ("Flags: ");
      if (!gets(reply))
	goto again;
      in->coda_open.flags = atoi(reply);
      break;

    case CODA_CLOSE:
      n = sizeof(struct inputArgs);
      if (getvfid(&in->coda_close.VFid) == -1)
	goto again;
      printf ("Flags: ");
      if (!gets(reply))
	goto again;
      in->coda_close.flags = atoi(reply);
      break;

    case CODA_CREATE:
      n = sizeof(struct inputArgs);
      printf ("Parent: ");
      if (getvfid(&in->coda_create.VFid) == -1)
	goto again;
      in->coda_create.attr.va_mode = 0666;
      in->coda_create.mode = 0666;
      in->coda_create.name = n;
      printf ("Name: ");
      if (!gets(&buf[n]))
	goto again;
      n += strlen(&buf[n]) + 1;
      break;

#if 0 /* no longer used */
    case CODA_READDIR:
      n = sizeof(struct inputArgs);
      if (getvfid(&in->coda_readdir.VFid) == -1)
	goto again;
      in->coda_readdir.count = 50;
      in->coda_readdir.offset = 0;
      break;
#endif

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

