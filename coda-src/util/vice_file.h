/* vice_file.h: prototypes

*/

#ifndef _VICE_FILE_H
#define _VICE_FILE_H

#ifdef __cplusplus
extern "C" {
#endif

void vice_dir_init(const char *dirname, int serverno);
const char *vice_file(const char *name);
const char *vice_sharedfile(const char *name);

#ifdef __cplusplus
}
#endif

#endif
