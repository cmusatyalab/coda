/* vice_file.h: prototypes

*/

#ifndef _VICE_FILE_H
#define _VICE_FILE_H

#ifdef __cplusplus
extern "C" {
#endif

void vice_dir_init (char *dirname, int serverno);
char * vice_file (char *name);
char * vice_sharedfile (char *name);

#ifdef __cplusplus
}
#endif

#endif
