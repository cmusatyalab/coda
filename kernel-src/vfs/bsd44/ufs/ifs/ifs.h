/*
 * ifs.h: definitions for the inodefs system calls 
 */

#ifndef _IFS_H_
#define _IFS_H_

#ifndef _KERNEL

extern int icreate __P((int, int, int, int, int, int));
extern int iopen   __P((int, int, int));
extern int iread   __P((int, int, long, unsigned int, char *, unsigned int));
extern int iwrite  __P((int, int, long, unsigned int, char *, unsigned int));
extern int iinc    __P((int, int, long));
extern int idec    __P((int, int, long));
extern int pioctl  __P((char *, int, caddr_t, int));

#else /* _KERNEL */

extern struct mount *devtomp __P((dev_t));

#endif /* _KERNEL */

#endif /* _IFS_H_ */
