#ifndef _DS_LOG_PRIVATE_H_
#define _DS_LOG_PRIVATE_H_

extern const magic_t ds_log_magic;

struct ds_log_t {
    magic_t magic;
    FILE *fp;
    int log_level;
    int flush_level;
    int oldyear;
    int oldday;
    int taglen;
    char *tag;
};

#define DS_LOG_VALID(lp) ((lp) && ((lp)->magic == ds_log_magic))

#endif /* _DS_LOG_PRIVATE_H_ */
