/* vice_file.h: prototypes

*/

#ifndef _VICE_FILE_H
#define _VICE_FILE_H

#ifdef __cplusplus
extern "C" {
#endif

void vice_dir_init(const char *dirname);
const char *vice_config_path(const char *name);

#ifdef __cplusplus
}
#endif

#endif
