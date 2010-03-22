/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-2010 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/*
*
*               rvmutl: RVM utility for log and segment maintainance
*
*/

#include <sys/file.h>                   /* for log, segment i/o */
#include <errno.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>
#include <stdio.h>                      /* not used for log, segment i/o */
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include "rvm_private.h"
#ifdef RVM_LOG_TAIL_BUG
#include <rvmtesting.h>
#endif

/* global variables */

extern rvm_bool_t   rvm_inited;         /* initialization complete flag */
extern rvm_bool_t   rvm_utlsw;          /* RVM utility flag */
extern
    device_t        *rvm_errdev;        /* last device reportiing error */
extern log_t        *default_log;       /* default log descriptor ptr */
extern
    rvm_length_t    page_size;          /* page length in bytes */
extern
    rvm_length_t    page_mask;          /* mask for rounding down to page size */
extern char         *rvm_errmsg;        /* internal error message ptr */

extern
    chk_vec_t       *rvm_chk_vec;       /* monitor range vector */
extern
    rvm_length_t    rvm_chk_len;        /* length of monitor range vector */
extern
    rvm_monitor_call_t *rvm_monitor;    /* monitor call-back function ptr */
extern
    rvm_signal_call_t *rvm_chk_sigint;  /* SIGINT handler call function ptr */
extern rvm_bool_t   rvm_no_update;      /* no segment or log update if true */
extern rvm_bool_t   rvm_replay;         /* true if recovery is a replay */

extern char         rvm_version;        /* interface version string from library */
extern char         rvm_log_version;    /* log format version from library */
extern char         rvm_statistics_version; /* statistics format from library */

/* constants */
#define CMD_MAX     2048                /* maximum cmd line length */
/* owns */
static log_t        *log;               /* log descriptor */
static device_t     *log_dev;           /* log device descriptor */
static log_status_t *status;            /* log status decsriptor */
static log_buf_t    *log_buf;           /* log i/o buffer */
static char         status_io[LOG_DEV_STATUS_SIZE]; /* status io area
                                           used by read_log_status so version
                                           info can be kept to print */
static rvm_offset_t save_prev_log_tail; /* previous log tail as read */
static long         args;               /* args read from argv */
static long         eof;                /* end of file flag */
static long         mode = 0644;        /* file create mode */
static char         dev_str[MAXPATHLEN+1]; /* file or device name string */
static FILE         *in_stream;         /* input stream */
static char         cmd_line[CMD_MAX];  /* command line buffer */
static char         *cmd_cur;           /* ptr to current command line
                                           position */
static char         sticky_buf[CMD_MAX]; /* buffer for sticky cmd lines */
static char         temp_str[CMD_MAX];  /* temporary string */
static rvm_length_t cur_rec_num;        /* current record number */
static rvm_offset_t cur_offset;         /* current position in log */
static struct timeval cur_timestamp;    /* timestamp of current record */
static struct timeval cur_trans_uname;  /* uname of current transaction */
static struct timeval cur_trans_commit; /* timestamp of current transaction
                                           commit */
static rvm_bool_t   cur_direction = FORWARD; /* current scan direction */
static rvm_bool_t   no_rec = rvm_true;  /* no record found in last search */
static rvm_bool_t   is_sticky_line;     /* currently processing repeated line */
static rvm_bool_t   stop_sticky;        /* cmd sets to stop sticky processing */
static rvm_bool_t   stop_sw = rvm_false;/* print stop, set by SIGINT handler */

static FILE         *monitor_out = NULL; /* output file for monitoring */
static FILE         *monitor_err = NULL; /* monitor error stream */
static rvm_length_t chk_alloc_len = 0;   /* monitor range vector allocation len */
static rvm_bool_t   monitor_vm;         /* true if monitor data is in vm  */

/* peek & poke buffer */
typedef struct
    {
    device_t        dev;                /* device desriptor */
    char            *buf;               /* i/o buffer */
    long            ptr;                /* buffer pointer */
    long            length;             /* buffer length */
    long            r_length;           /* read length */
    rvm_offset_t    offset;             /* current offset in file */
    rvm_offset_t    end_offset;         /* current end offset in file */
    rvm_offset_t    sticky_offset;      /* last offset printed */
    }
peekpoke_buf_t;

static peekpoke_buf_t peekpoke;         /* file & buffer for peek/poke cmds */

/* buffer loading function type */
typedef char *chk_buffer_t();
/*  rvm_offset_t    *offset;
    rvm_length_t    length;
    FILE            *err_stream;
*/
/* numeric size and format switches */
typedef enum
    {
    unknown_flag = -123456789,          /* unknown flag */

    byte_sw = 1,                        /* bytes */
    char_sw = 2,                        /* characters */
    dec_sw = 4,                         /* decimal */
    float_sw = 8,                       /* floating point */
    long_sw = 16,                       /* long */
    octal_sw = 32,                      /* octal */
    offset_sw = 64,                     /* rvm_offset_t */
    short_sw = 128,                     /* short */
    tv_sw = 256,                        /* time stamp */
    unsigned_sw = 512,                  /* unsigned */
    hex_sw = 1024,                      /* hexadecimal */
    double_sw = 2048                    /* double precision floating point */
    }
format_flag_id;

#define ALL_SIZES   ((unsigned long)(byte_sw | long_sw | short_sw \
                                     | offset_sw | float_sw \
                                     | double_sw | tv_sw))
#define ALL_FORMATS ((unsigned long)(char_sw | dec_sw | octal_sw \
                                     | hex_sw | unsigned_sw | float_sw\
                                     | double_sw | offset_sw | tv_sw))

static unsigned long num_format;        /* bit vector of scanned format switches */
static long num_count;                  /* repeat count for data printing */
static rvm_bool_t num_all;                    /* print all data if true */
/* string name lookup entry declarations */
#define STR_NAME_LEN 31                 /* maximum length of string name */

typedef enum
    {
    UNKNOWN = -123456789,               /* unknown string key */

    ALL_KEY = 1,                        /* key for do all entities */
    CLEAR_KEY,                          /* function clear key */
    FILE_KEY,                           /* file spec key */
    PAGE_KEY,                           /* key for page size */
    NUM_KEY,                            /* key for numeric parameter */
    REC_KEY,                            /* record number key */
    TIME_KEY,                           /* timestamp key */
    UNAME_KEY,                          /* uname key */
    HEAD_KEY,                           /* log/list head key */
    TAIL_KEY,                           /* log/list tail key */
    NO_TAIL_KEY,                        /* suppress tail processing key */
    LOG_START_KEY,                      /* start of data area key */
    LOG_STATUS_KEY,                     /* log status area key */
    MODS_KEY,                           /* modifications key */
    MONITOR_KEY,                        /* vm address monitoring key */
    NEXT_KEY,                           /* next entry key */
    PREV_KEY,                           /* previous entry key */
    NEXT_SUB_KEY,                       /* next sub entry key */
    SUB_KEY,                            /* current sub entry key */
    PREV_SUB_KEY,                       /* previous sub entry key */
    PREV_HEAD_KEY,                      /* previous log head key */
    PREV_TAIL_KEY,                      /* previous log tail key */
    EARLY_KEY,                          /* earliest record key */
    REMAINING_KEY,                      /* remaining records key */
    REC_END_KEY,                        /* record end key */
    REC_HDR_KEY,                        /* record header key */
    READ_ONLY_KEY,                      /* read only key */
    UPDATE_KEY,                         /* update key */
    NO_UPDATE_KEY,                      /* no update key */
    REPLAY_KEY,                         /* recovery replay key */
    SEG_DICT_KEY,                       /* segment dictionary key */
    STATISTICS_KEY,                     /* statistics area key */

    NOT_A_KEY                           /* end mark, do not delete */
    }
key_id_t;

typedef rvm_bool_t cmd_func_t();              /* command function template */
typedef rvm_bool_t sw_func_t();               /* switch processing function template */

typedef struct                          /* string name vector entry */
    {
    char            str_name[STR_NAME_LEN+1];  /* character string for name */
    char            *target;            /* target object */
    cmd_func_t      *func;              /* target function */
    rvm_bool_t      sticky;             /* command retained if true */
    }
str_name_entry_t;
#define STICKY      rvm_true
/* macros */

/* cmd line cursor incrementor */
#define incr_cur(_i) cmd_cur = &cmd_cur[_i]

/* tests for end of line characters */
#define end_line(c) (((c) == '\0') || ((c) == '\n'))
#define scan_stop(c) (end_line(c) || ((c) == '>') || ((c) == '#'))

/* test for octal digits */
#define isoctal(c)  ((isdigit(c) \
                      && !(((c) == '8') || ((c) == '9'))))
/* other constants */

#define LINE_WIDTH  80                  /* length of print line */
#define MAX_CHARS   255                 /* maximum nuber of characters to print */
#define DATA_END_STR        "** end of data **"
#define EOF_STR             "** end of file **"
#define INIT_BUF_STR        "initializing buffer"
#define LOG_NAME_PROMPT     "Enter name of log file or device:"
#define LOG_SCAN_STR        "scanning log"
#define NOT_MEANINGFUL_STR  "not meaningful in this context"
#define READ_STATUS_STR     "reading log status"
#define WRITE_STATUS_STR    "writing log status"
#define KEY_WORD_STR        "key word"

/* internal forward declarations */
extern rvm_bool_t do_quit();
extern rvm_bool_t chk_sigint();
static long lookup_str_name();

#ifndef ZERO
#define ZERO 0
#endif

/* scanner utilities */

static void skip_white(ptr)
    char            **ptr;
    {
    while (isspace(**ptr))
        (*ptr)++;
    }

static void skip_lf()
    {
    while (rvm_true)
        if (getc(stdin) == '\n') return;
    }

static unsigned intval(c)
    char            c;
    {
    if (isdigit(c))                     /* octal & decimal */
        return (unsigned)c - (unsigned)'0';
    
    if (isupper(c))
        return (unsigned)c - (unsigned)'A' + 10;
    else
        return (unsigned)c - (unsigned)'a' + 10;
    }

#ifdef UNUSED_FUNCTIONS
/* character substitution: locate all occurances of c1, change to c2 */
static void change_to(c1,c2,str)
    char            c1;                 /* target character */
    char            c2;                 /* substitute character */
    char            *str;               /* string */
    {
    long            i = 0;

    while (str[i] != '0')
        {
        if (str[i] == c1)
            str[i] = c2;
        i++;
        }
    }
#endif

/* similar to ANSI function, but returns base used */
static unsigned long str2ul(str,ptr,base_used)
    char            *str;               /* ptr to conversion string */
    char            **ptr;              /* ptr to terminating char [out] */
    int             *base_used;         /* base used for conversion [out] */
    {
    int             base = 0;
    unsigned long   l = 0;
    
    skip_white(&str);
    if (base_used != NULL) *base_used = 0;
    if ((isdigit(*str)) && (*str != '0')) base = 10;
    else
        {
        if (*str == '0') 
            {
            base = 8; str++;
            }
        if ((*str == 'x') || (*str == 'X'))
            {
            base = 16; str++;
            }
        else if (base == 0) goto exit;
        }

    DO_FOREVER
        {
        switch (base)
            {
          case 8:  if ((!isdigit(*str)) || (*str=='8')
                       || (*str=='9')) goto exit;
            break;
          case 10: if (!isdigit(*str)) goto exit;
            break;
          case 16: if (!isxdigit(*str)) goto exit;
            break;
          default: assert(rvm_false);
            }

        l = l*base + intval(*str);
        str++;
        }
exit:
    if (ptr != NULL) *ptr = str;
    if (base_used != NULL)
        *base_used = base;
    return l;
    }
/* similar to ANSI function, but returns base used */
static long str2l(str,ptr,base_used)
    char            *str;               /* ptr to conversion string */
    char            **ptr;              /* ptr to terminating char [out] */
    int             *base_used;         /* base used for conversion [out] */
    {
    long            l;
    rvm_bool_t      is_neg = rvm_false;

    skip_white(&str);
    if (*str == '-')
        {
        is_neg = rvm_true;
        str++;
        }

    l = str2ul(str,ptr,base_used);
    if (is_neg) return -l;
    else return l;
    }

/* rvm_offset_t scanner */
static rvm_offset_t str2off(str,ptr,base_used)
    char            *str;               /* ptr to conversion string */
    char            **ptr;              /* ptr to terminating char [out] */
    int             *base_used;         /* base used for conversion [out] */
    {
    rvm_length_t    high;
    rvm_length_t    low;

    high = str2ul(str,ptr,base_used);
    skip_white(ptr);
    if (*str == ',')
        {                               /* low order value follows */
        str++;
        low = str2ul(str,ptr,base_used);
        }
    else
        {
        low = high;                     /* first (only) value was low order */
        high = 0;
        }

    return RVM_MK_OFFSET(high,low);
    }
/* floating pt. number tester */
static int is_float(str,out_ptr)
    char            *str;               /* ptr to conversion string */
    char            **out_ptr;          /* ptr to terminating char [out] */
    {
    int             retval = -1;        /* -1: not numeric, 0: int, 1: float */
    char            *ptr = str;         /* working ptr to input string */
    char            c;

    DO_FOREVER
        {
        if (isdigit((c = *(ptr++))))
            {
            if (retval < 0) retval = 0;
            continue;
            }
        switch (c)
            {
          case '.': case 'E': case 'e':
            retval = 1; continue;
          case '-': case '+': continue;
            }
        break;
        }

    if (out_ptr != NULL) *out_ptr = ptr;
    return retval;
    }

/* double float scanner */
static double str2dbl(str,ptr)
    char            *str;               /* ptr to conversion string */
    char            **ptr;              /* ptr to terminating char [out] */
    {
    double          val = 0.0;          /* value scanned */

    if (is_float(str,ptr) >= 0)
        sscanf(str,"%lf",&val);

    return val;
    }
/* string scanner */
static long scan_str(str,len)
    char            *str;               /* ptr to string buffer */
    long            len;                /* maximum length */
    {
    long            indx = 0;

    skip_white(&cmd_cur);
    while (!scan_stop(*cmd_cur) && (!isspace(*cmd_cur))
           && (indx < len))
        str[indx++] = *(cmd_cur++);

    str[indx] = '\0';

    return indx;                        /* return length */
    }
/* scan quoted string, uses C escape conventions */
static long scan_qstr(str,len)
    char            *str;               /* ptr to string buffer */
    long            len;                /* maximum length */
    {
    long            indx = 0;           /* string index/length */
    char            ostr[5];            /* octal conversion string */
    char            c;

    if (*cmd_cur == '\"') incr_cur(1);
    while (!end_line(*cmd_cur) && (indx < len))
        {
        if (*cmd_cur == '\"')
            {
            incr_cur(1); break;         /* end of string */
            }

        /* check for escape sequences */
        if ((c = *(cmd_cur++)) != '\\')
            str[indx++] = c;            /* no conversion needed */
        else
            if (isoctal(*cmd_cur))      /* convert octal numerics */
                {
                (void)strcpy(ostr,"0   ");
                ostr[1] = *(cmd_cur++);
                if (isoctal(*cmd_cur))
                    {
                    ostr[2] = *(cmd_cur++);
                    if (isoctal(*cmd_cur))
                        ostr[3] = *(cmd_cur++);
                    }
                str[indx++] =  str2ul(ostr,NULL,NULL);
                }
            else
                switch ((c = *(cmd_cur++))) /* convert other escapes */
                    {
                  case '\\': str[indx++] = '\\'; break;
                  case '\'': str[indx++] = '\''; break;
                  case '\"': str[indx++] = '\"'; break;
                  case '\b': str[indx++] = '\b'; break;
                  case '\f': str[indx++] = '\f'; break;
                  case '\n': str[indx++] = '\n'; break;
                  case '\r': str[indx++] = '\r'; break;
                  case '\t': str[indx++] = '\t'; break;
                  case '\v': str[indx++] = '\v'; break;
                  default: str[indx++] = c; /* null conversion */
                    }
        }
    str[indx] = '\0';
    return indx;                        /* return length */
    }
/* string name scan and lookup */
static long scan_str_name(str_name,str_vec,ambig_str)
    char            *str_name;          /* string name buffer */
    str_name_entry_t *str_vec;          /* defining vector */
    char            *ambig_str;         /* ambiguous name type */
    {
    char            str_name_buf[STR_NAME_LEN+1]; /* name buffer */
    char            *buf_ptr;

    /* scan string name, whitesp as seperators */
    skip_white(&cmd_cur);
    if (scan_stop(*cmd_cur))
        return (long)UNKNOWN;           /* all done ?? */
    
    /* scan and lookup name */
    if (str_name != NULL) buf_ptr = str_name;
    else buf_ptr = str_name_buf;
    (void)scan_str(buf_ptr,STR_NAME_LEN);
    return lookup_str_name(buf_ptr,str_vec,ambig_str);
    }
/* string name lookup: accepts minimum substring for match */
static long lookup_str_name(str,str_vec,ambig_str)
    char            *str;               /* name to lookup */
    str_name_entry_t *str_vec;          /* defining vector */
    char            *ambig_str;         /* ambiguous name type */
    {
    long            i = 0;              /* loop counter */
    long            str_index = (long)UNKNOWN; /* string index in vector */

    while (str_vec[i].str_name[0] != '\0')
        {
        /* test if name string starts with str */
        if (strstr(str_vec[i].str_name,str) ==
            str_vec[i].str_name)
            {
            /* yes, candidate found -- test further */
            if (strcmp(str_vec[i].str_name,str) == 0)
                return i;               /* exact match, select this name */
            if (str_index == (long)UNKNOWN)
                str_index = i;          /* substring match, remember name */
            else
                {                       /* error: ambigous name */
                if (ambig_str != NULL)
                    fprintf(stderr,"? %s: ambiguous %s: %s or %s?\n",
                            str,ambig_str,str_vec[i].str_name,
                            str_vec[str_index].str_name);
                return (long)UNKNOWN;
                }
            }
        i++;
        }

    /* see if name not found */
    if ((str_index == (long)UNKNOWN) && (ambig_str != NULL))
        fprintf(stderr,"? %s: %s not found\n",str,ambig_str);

    return str_index;
    }

static void bad_key_word(key_word,key_msg,err_stream)
    char            *key_word;
    char            *key_msg;
    FILE            *err_stream;
    {
    
    fprintf(err_stream,"\n? Key word \"%s\" %s\n",key_word,key_msg);

    }
/* numeric format print/scan support */
static void init_num_format()
    {
    num_count = 1;
    num_all = rvm_false;
    num_format = (unsigned long)(dec_sw | long_sw);
    }

static void bad_num_format(err_stream)
    FILE            *err_stream;
    {
    fprintf(err_stream,"\n? Unknown numeric format code: %c\n",*cmd_cur);
    }

static long num_format_size(format)
    unsigned long   format;
    {
    switch (format & ALL_SIZES)
        {
      case byte_sw:
        return sizeof(char);
      case short_sw:
        return sizeof(short);
      case long_sw:
        return sizeof(long);
      case offset_sw:
        return sizeof(rvm_offset_t);
      case double_sw:
        return sizeof(double);
      case float_sw:
        return sizeof(double);
      case char_sw:
        return sizeof(char);
      case tv_sw:
        return sizeof(struct timeval);
      default:          assert(rvm_false);
        }

    return 0;
    }
/* format alignment checkers */
static rvm_bool_t num_format_align(format,addr)
    int             format;
    char            *addr;
    {
    int             size;

    size = num_format_size(format);
    /* check alignment for access of format type */
    if ((size == 1) || ((rvm_length_t)addr % size) == 0)
        return rvm_true;                /* OK */
    else
        return rvm_false;               /* error */
    }

static void bad_format_align(err_stream,format)
    FILE            *err_stream;
    int             format;
    {

    /* alignement error */
    fprintf(err_stream,"\n?  Alignment error for access of type ");
    switch (format & ALL_SIZES)
        {
      case short_sw:
        fprintf(err_stream,"short\n"); return;
      case long_sw:
        fprintf(err_stream,"long\n"); return;
      case offset_sw:
        fprintf(err_stream,"rvm_offset_t\n"); return;
      case double_sw:
        fprintf(err_stream,"double\n"); return;
      case float_sw:
        fprintf(err_stream,"float\n"); return;
      case tv_sw:
        fprintf(err_stream,"struct timeval\n"); return;
      case char_sw: case byte_sw:
      default:          assert(rvm_false);
        }
    }
/* numeric size and format switch scanner */
static rvm_bool_t scan_num_format()
    {
    unsigned long   temp;

    incr_cur(1);
    skip_white(&cmd_cur);
    init_num_format();

    /* get count if specified; otherwise set to 1 */
    if (isdigit(*cmd_cur))
        num_count = str2l(cmd_cur,&cmd_cur,NULL);
    if (num_count < 1) num_count = 1;

    /* get size and format */
    DO_FOREVER
        {
        switch (*cmd_cur)
            {
          case '\0': case '>': case ' ': case '\n': case '#':
              return rvm_true;   /* all done */
          case 'b':
            num_format &= ~ALL_SIZES;
            num_format |= (unsigned long)byte_sw;
            break;
          case 'c':
            num_format &= ~ALL_SIZES;
            num_format &= ~ALL_FORMATS;
            num_format |= (unsigned long)(byte_sw | char_sw);
            break;
          case 'd':
            temp = num_format & (unsigned long)unsigned_sw;
            num_format &= ~ALL_FORMATS;
            num_format |= ((unsigned long)dec_sw | temp);
            break;
          case 'D':
            num_format &= ~ALL_SIZES;
            num_format &= ~ALL_FORMATS;
            num_format |= (unsigned long)double_sw;
            break;
          case 'f': 
            num_format &= ~ALL_SIZES;
            num_format &= ~ALL_FORMATS;
            num_format |= (unsigned long)float_sw;
            break;
          case 'h':
            num_format &= ~ALL_SIZES;
            num_format &= ~ALL_FORMATS;
            num_format |= (unsigned long)(short_sw | dec_sw); 
            break;
          case 'i':
            num_format &= ~ALL_SIZES;
            num_format &= ~ALL_FORMATS;
            num_format |= (unsigned long)(long_sw | dec_sw); 
            break;
          case 'l':
            num_format &= ~ALL_SIZES;
            num_format |= (unsigned long)long_sw; 
            break;
          case 'L':
            num_format &= ~ALL_SIZES;
            num_format &= ~ALL_FORMATS;
            num_format |= (unsigned long)(long_sw | unsigned_sw
                                          | dec_sw);
            break;
          case 'o':
            num_format &= ~ALL_FORMATS;
            num_format |= (unsigned long)(octal_sw | unsigned_sw);
            break;
          case 'O':
            num_format &= ~ALL_SIZES;
            num_format &= ~ALL_FORMATS;
            num_format |= (unsigned long)(offset_sw);
            break;
          case 's':
            num_format &= ~ALL_SIZES;
            num_format |= (unsigned long)short_sw;
            break;
          case 't':
            num_format &= ~ALL_SIZES;
            num_format &= ~ALL_FORMATS;
            num_format |= (unsigned long)(tv_sw);
            break;
          case 'u': 
            num_format &= ~ALL_FORMATS;
            num_format |= (unsigned long)unsigned_sw;
            break;
          case 'w':
            num_format &= ~ALL_SIZES;
            num_format &= ~ALL_FORMATS;
            num_format |= (unsigned long)(long_sw | dec_sw);
            break;
          case 'x': 
            num_format &= ~ALL_FORMATS;
            num_format |= (unsigned long)(hex_sw | unsigned_sw);
            break;
          case '*':
            num_all = rvm_true;
            break;
          default:
            return rvm_false;
            }
        incr_cur(1);
        }
    }
/* print format descriptor */
static void pr_format(out_stream,format,count,all)
    FILE            *out_stream;        /* target stream */
    unsigned long   format;             /* format to print */
    long            count;              /* count to print */
    rvm_bool_t      all;
    {
    unsigned long   test;

    /* print format slash and count */
    putc('/',out_stream);
    if (all)
        putc('*',out_stream);
    else
        fprintf(out_stream,"%ld",count);
    
    /* print format swithes */
    switch (test = (format & ALL_FORMATS))
        {
        /* special size/formats */
      case char_sw:     putc('c',out_stream); return;
      case double_sw:   putc('D',out_stream); return;
      case float_sw:    putc('f',out_stream); return;
      case offset_sw:   putc('O',out_stream); return;
      case tv_sw:       putc('t',out_stream); return;
        
        /* formats */
      case (unsigned long)(unsigned_sw | hex_sw): case hex_sw:
          putc('x',out_stream); break;
      case (unsigned long)(unsigned_sw | octal_sw): case octal_sw: 
          putc('o',out_stream); break;
      case (unsigned long)(unsigned_sw | dec_sw): case unsigned_sw:
          putc('u',out_stream);
      case dec_sw:      putc('d',out_stream);
        }

    switch (format & ALL_SIZES)
        {
      case byte_sw:     putc('b',out_stream); return;
      case short_sw:    putc('s',out_stream); return;
      case long_sw:     putc('l',out_stream); return;
        }

    }
/* print fixed-width unsigned long */
static void pr_ulong(out_stream,ul,base,width)
    FILE            *out_stream;        /* target stream */
    rvm_length_t    ul;                 /* data to print */
    int             base;               /* conversion base (8,10,16) */
    int             *width;             /* printed width [out] */
    { 
    switch (base)
        {
      case 8:
        fprintf(out_stream,"%0#11lo",ul);
        if (width != NULL) *width += 12;
        break;
      case 16:
        fprintf(out_stream,"%0#8lx",ul);
        if (width != NULL) *width += 10;
        break;
      case 10: default:
        fprintf(out_stream,"%10.1lu",ul);
        if (width != NULL) *width += 10;
        }
    }
/* print full width rvm_offset_t */
static void prw_offset(out_stream,offset,base,width)
    FILE            *out_stream;        /* target stream */
    rvm_offset_t    *offset;            /* ptr to data */
    int             base;               /* conversion base (8,10,16) */
    int             *width;             /* printed width [out] */
    {
    pr_ulong(out_stream,RVM_OFFSET_HIGH_BITS_TO_LENGTH(*offset),
             base,width);
    switch (base)
        {
      case 8:
        fprintf(out_stream,", %011lo",RVM_OFFSET_TO_LENGTH(*offset));
        if (width != NULL) *width += 13;
        break;
      case 16:
        fprintf(out_stream,", %08lx",RVM_OFFSET_TO_LENGTH(*offset));
        if (width != NULL) *width += 10;
        break;
      case 10: default:
        fprintf(out_stream,", %010lu",RVM_OFFSET_TO_LENGTH(*offset));
        if (width != NULL) *width += 12;
        }
    }

/* In some places we were using prw_offset(..., (rvm_offset_t *)&tv, 10, NULL);
 * this is uses the same formatting but avoids the strict aliasing warnings */
static void prw_timeval(FILE *out_stream, struct timeval *tv)
{
    rvm_offset_t offset = { .high = tv->tv_sec, .low = tv->tv_usec };
    prw_offset(out_stream, &offset, 10, NULL);
}

/* print compact rvm_offset_t */
static void prc_offset(out_stream,offset,base,width)
    FILE            *out_stream;        /* target stream */
    rvm_offset_t    *offset;            /* ptr to data */
    int             base;               /* conversion base (8,10,16) */
    int             *width;             /* printed width [out] */
    {
    if (RVM_OFFSET_HIGH_BITS_TO_LENGTH(*offset) != 0)
        prw_offset(out_stream,offset,base,width);
    else
        pr_ulong(out_stream,RVM_OFFSET_TO_LENGTH(*offset),
                 base,width);
    }
/* print "true" or "false" */
static void pr_bool(out_stream,b,width)
    FILE            *out_stream;        /* target stream */
    rvm_bool_t      b;                  /* rvm_bool_t value */
    int             *width;             /* printed width [out] */
    {
    if (b)
        fprintf(out_stream," true");
    else
        fprintf(out_stream,"false");
        if (width != NULL) *width += 5;
    }

/* print printable characters, C escapes, octal otherwise */
static void pr_char(out_stream,c,width)
    FILE            *out_stream;        /* target stream */
    char            c;                  /* character to print */
    int             *width;             /* printed width [out] */
    {
    if (isprint(c))
        putc(c,out_stream);
    else
        {
        putc('\\',out_stream);
        if (width != NULL) *width += 1;
        switch (c)
            {
          case '\b': putc('b',out_stream); break;
          case '\f': putc('f',out_stream); break;
          case '\n': putc('n',out_stream); break;
          case '\r': putc('r',out_stream); break;
          case '\t': putc('t',out_stream); break;
          case '\v': putc('v',out_stream); break;
          default:
            fprintf(out_stream,"%3o",(int)c&0377);
            if (width != NULL) *width += 3;
            return;
            }
        }

    if (width != NULL) *width += 1;
    }
/* timestamp print support -- similar to ctime, but also can print usecs */

static char         *day_vec[7] = {"Sun","Mon","Tue","Wen","Thu","Fri","Sat"};
static char         *mon_vec[12] = {"Jan","Feb","Mar","Apr","May","Jun",
                                    "Jul","Aug","Sep","Oct","Nov","Dec"};
static void pr_timeval(out_stream,timestamp,usec,width)
    FILE            *out_stream;        /* target stream */
    struct timeval  *timestamp;         /* timeval to print */
    rvm_bool_t      usec;               /* print microsecs if true */
    int             *width;             /* printed width [out] */
    {
    struct tm       *tfields;           /* broken down timeval struct */

    tfields = localtime((long *)timestamp);

    fprintf(out_stream,"%3s ",day_vec[tfields->tm_wday]);
    fprintf(out_stream,"%3s ",mon_vec[tfields->tm_mon]);
    fprintf(out_stream,"%2d ",tfields->tm_mday);
    fprintf(out_stream,"%4d ",(1900+tfields->tm_year));
    fprintf(out_stream,"%2d:",tfields->tm_hour);
    fprintf(out_stream,"%02d:",tfields->tm_min);
    fprintf(out_stream,"%02d",tfields->tm_sec);
    if (width != NULL) *width += 24;

    /* print usec as decimal fraction after seconds if requested */
    if (usec)
        {
        fprintf(out_stream,".%06lu",timestamp->tv_usec);
        if (width != NULL) *width += 7;
        }
    }
/* RVM error printing */
static void pr_rvm_error(err_stream,errcode,msg)
    rvm_return_t    errcode;            /* error code */
    FILE            *err_stream;        /* error stream */
    char            *msg;               /* error message */
    {

    switch (errcode)
        {
      case RVM_EIO:
        fprintf(err_stream,"\n? I/O error from %s %s: %d\n",
                rvm_errdev->name,msg,errno);
        return;
      case RVM_EINTERNAL:
        fprintf(err_stream,"? Internal error %s: %s\n",
                msg,rvm_errmsg);
        return;
      default:
        fprintf(err_stream,"? Error %s, return code: %s\n",
                msg,rvm_return(errcode));
        }

    }
/* formatted memory printer */
static void pr_memory(out_stream,err_stream,addr,count,
                      tot_size,tot_width)
    FILE            *out_stream;        /* target stream */
    FILE            *err_stream;        /* target error stream */
    char            *addr;              /* location of data */
    rvm_length_t    count;              /* repeat count */
    rvm_length_t    *tot_size;          /* size of memory item [out] */
    int             *tot_width;         /* print width [out] */
    {
    unsigned long   format;
    long            i = 0;              /* memory offset */
    long            val=0;              /* integer value */
    long            size;               /* size of memory item */
    int             base=0;             /* base for offset conversion */
    int             width;              /* print width */

    /* get format and size for data; check alignment */
    format = (num_format & ALL_FORMATS)
        & ~(unsigned long)unsigned_sw;
    size = num_format_size(num_format);
    if (!num_format_align(num_format,addr))
        {
        bad_format_align(err_stream,num_format);
        return;
        }

    /* figure out width */
    switch (num_format & ALL_SIZES)
        {
      case char_sw:
      case byte_sw:
        val = addr[i]; width = 3;
        if (num_format & (unsigned long)unsigned_sw)
            val = val & 0377; break;
      case short_sw:
        val = *((short *)&addr[i]); width = 5;
        if (num_format & (unsigned long)unsigned_sw)
            val = val & 0177777; break;
      case long_sw:
        val = *((long *)&addr[i]); width = 12; break;
      case offset_sw:
        switch (num_format & (unsigned long)hex_sw
                & (unsigned long)octal_sw)
            {
          case octal_sw: base = 8; break;
          case hex_sw: base = 16; break;
          default: base = 10;
            }
        break;
      case double_sw: case float_sw:
        break;
      default:          assert(rvm_false);
        }

    /* print with requested format */
    while (count-- > 0)
        {
        switch (format)
            {                           /* determine format and print */
          case dec_sw:
            if (num_format & (unsigned long)unsigned_sw)
                fprintf(out_stream,"%*lu",width,val);
            else
                fprintf(out_stream,"%*ld",width,val);
            break;
          case octal_sw:
            width = 3*size+1;
            fprintf(out_stream,"%0#*lo",width,val);
            break;
          case hex_sw:
            width = 2*size+2;
            fprintf(out_stream,"%0#*lx",width,val);
            break;
          case float_sw:
            width = 13;
            fprintf(out_stream,"% 13.6E",*((float *)&addr[i]));
            break;
          case double_sw:
            width = 17;
            fprintf(out_stream,"% 17.10E",*((double *)&addr[i]));
            break;
          case offset_sw:
            width = 0;
            prc_offset(out_stream,(rvm_offset_t *)&addr[i],
                       base,&width);
            break;
          case char_sw:
            if (addr[i] == '\0') return;
            width = 0;
            pr_char(out_stream,addr[i++],&width);
            break;
          case tv_sw:
            width = 0;
            pr_timeval(out_stream,(struct timeval *)&addr[i],
                       rvm_true,&width);
            break;
          default: assert(rvm_false);
            }
        if ((count != 0) && (format != (unsigned long)char_sw))
            {
            fprintf(out_stream," ");    /* print sperating space if not done */
            (*tot_width)++;
            }
        /* tally size and width processed */
        i += size; *tot_size += size;
        *tot_width += width;
        }
    }
/* offset range printer ,*/
static rvm_bool_t pr_data_range(out_stream,err_stream,indent,line_width,
                          offset,pr_offset,base,count,chk_buffer,
                          limit_offset,limit_str)
    FILE            *out_stream;        /* output stream */
    FILE            *err_stream;        /* error output stream */
    int             indent;             /* num spaces to indent */
    int             line_width;         /* num characters in line */
    rvm_offset_t    *offset;            /* ptr to data offset [in/out] */
    rvm_offset_t    *pr_offset;         /* ptr to printed offset [in/out] */
    int             base;               /* radix of pr_offset */
    rvm_length_t    count;              /* number of memory items to print */
    chk_buffer_t    *chk_buffer;        /* buffer manager - returns ptr to data */
    rvm_offset_t    *limit_offset;      /* limit of valid data */
    char            *limit_str;         /* msg to print if limit exceeded */
    {
    char            *data_ptr;          /* ptr to data to print */
    rvm_length_t    tot_size;           /* total size of mem items printed */
    int             tot_width;          /* printed line width */
    int             i;

    while (count > 0)
        {
        /* check for print abort or alignment error */
        if (chk_sigint(out_stream)) return rvm_true;
        if ((data_ptr=chk_buffer(offset,SECTOR_SIZE,err_stream))
            == NULL) return rvm_false;
        if (!num_format_align(num_format,data_ptr))
            {
            bad_format_align(err_stream,num_format);
            return rvm_false;
            }

        /* print offset & what fits on line */
        tot_width = indent+1;
        for (i=1; i<=indent; i++) putc(' ',out_stream);
        prc_offset(out_stream,pr_offset,base,&tot_width);
        putc(':',out_stream);

        while ((count != 0) && (tot_width <= line_width))
            {/* see if will exceed limits of valid data */
            if (RVM_OFFSET_GTR(RVM_ADD_LENGTH_TO_OFFSET(*offset,
                    num_format_size(num_format)),*limit_offset))
                {
                if (!num_all)
                    fprintf(out_stream," %s",limit_str);
                putc('\n',out_stream);
                return rvm_true;
                }
            /* be sure memory item is in buffer */
            if ((data_ptr=chk_buffer(offset,SECTOR_SIZE,err_stream))
                == NULL) return rvm_false;

            /* if printing characters, see if null */
            if ((num_format & (unsigned long)char_sw)
                && (*data_ptr == '\0'))
                {
                count = 0; break;   /* done */
                }

            /* print next item and tally offsets and count */
            if (!(num_format & (long)char_sw))
                pr_char(out_stream,' ',&tot_width);
            tot_size = 0;
            pr_memory(out_stream,err_stream,data_ptr,1,
                      &tot_size,&tot_width);
            *offset = RVM_ADD_LENGTH_TO_OFFSET(*offset,tot_size);
            *pr_offset = RVM_ADD_LENGTH_TO_OFFSET(*pr_offset,tot_size);
            if (!num_all) count--;
            }
        putc('\n',out_stream);
        }

    return rvm_true;
    }
/* get boolean answer */
rvm_bool_t get_ans(prompt,sense)
    char            *prompt;            /* prompt string */
    rvm_bool_t      sense;              /* false if default is 'no' */
    {
    char            sense_char;         /* default sense character */

    if (sense) sense_char = 'y';
    else sense_char = 'n';

    while (rvm_true)
        {
        printf("%s (y or n [%c])? ",prompt,sense_char);
        switch (getc(stdin))
            {
          case 'y': case 'Y':
            skip_lf(); return rvm_true;
            
          case 'n': case 'N':
            skip_lf(); return rvm_false;

          case '\n':        return sense;

          default:          skip_lf();
            }
        }
    }
/* output redirection scanner */
static void get_redirect(out_stream,err_stream)
    FILE            **out_stream;
    FILE            **err_stream;
    {
    char            *cmd_cur_save;      /* input line ptr hide-away */
    long            file_name_len;      /* length of file name */
    rvm_bool_t      redir_err = rvm_false;  /* redirect stderr, too */
    rvm_bool_t      append = rvm_false;     /* append if file exits */
    FILE            *redir;             /* redirection file handle */

    cmd_cur_save = cmd_cur;             /* save incoming state */
    *out_stream = stdout;               /* set default streams */
    *err_stream = stderr;

    cmd_cur = strchr(cmd_cur,'>');
    if (cmd_cur == NULL) goto exit;     /* no redirection */

    /* redirection indicated, see what wanted */
    incr_cur(1);
    skip_white(&cmd_cur);
    if (*cmd_cur == '>')                /* append */
        {
        append = rvm_true;
        incr_cur(1);
        skip_white(&cmd_cur);
        }
    if ((*cmd_cur == '&') && (err_stream != NULL)) /* redirect stderr */
        {
        redir_err = rvm_true;
        incr_cur(1);
        skip_white(&cmd_cur);
        }

    /* get file name */
    file_name_len = scan_str(dev_str,MAXPATHLEN);
    if (file_name_len == 0)
        {
        fprintf(stderr,"? no file specified\n");
        goto exit;
        }
    /* open the file and assign to streams */
    if (append) redir = fopen(dev_str,"a");
    else        redir = fopen(dev_str,"w");
    if (redir != NULL)
        {
        *out_stream = redir;                   /* redirect std output */
        if (redir_err) *err_stream = redir;    /* redirect std error */
        }
    else
        pr_rvm_error(stderr,RVM_EIO,"opening redirected output file");
    
  exit:
    cmd_cur = cmd_cur_save;
    }
/* close redirected streams */
static void close_redirect(out_stream,err_stream)
    FILE            *out_stream;
    FILE            *err_stream;
    {

    if ((out_stream != stdout) && (out_stream != NULL))
        fclose(out_stream);

    if ((err_stream != stderr) && (err_stream != NULL))
        fclose(err_stream);
    }

static rvm_bool_t no_log()
    {
    if (log == NULL)
        {
        fprintf(stderr,"? log not open\n");
        return rvm_true;
        }
    return rvm_false;
    }
/* read prompted line from terminal */
static char *read_prompt_line(out_stream,prompt,null_ok)
    FILE            *out_stream;
    char            *prompt;            /* prompt string */
    rvm_bool_t      null_ok;            /* true if null input ok */
    {

    skip_white(&cmd_cur);
    if (chk_sigint(out_stream)) return NULL;

    /* read from input stream if nothing left in command line */
    if (scan_stop(*cmd_cur) && (! null_ok))
        DO_FOREVER                      
            {
            /* prompt if required */
            cmd_line[0] = '\0';
            if ((prompt != NULL) && (in_stream == stdin))
                fprintf(out_stream,"%s ",prompt);

            /* get line and check termination conditions */
            cmd_cur = fgets(cmd_line,CMD_MAX,in_stream);
            if ((cmd_cur == NULL) || chk_sigint(NULL))
                return NULL;            /* error */
            if ((null_ok) || (cmd_line[0] != '\0'))
                return cmd_cur=cmd_line;
            }
    return NULL;
    }
/* type sizes vector */
#define MAX_TYPES   100                 /* maximum number of types */

static str_name_entry_t type_vec[MAX_TYPES] = /* type sizes vector */
                    {
                    {"all",(char *)ALL_KEY},    /* not a type */
                    {"all_sizes",(char *)ALL_KEY}, /* not a type */
                    {"condition",(char *)sizeof(RVM_CONDITION)},
                    {"dev",(char *)sizeof(device_t)},
                    {"device_t",(char *)sizeof(device_t)},
                    {"dev_region_t",(char *)sizeof(dev_region_t)},
                    {"FLUSH_BUF_LEN",(char *)FLUSH_BUF_LEN},
                    {"free_page_t",(char *)sizeof(free_page_t)},
                    {"MAXPATHLEN",(char *)MAXPATHLEN},
                    {"int",(char *)sizeof(int)},
                    {"list_entry_t",(char *)sizeof(list_entry_t)},
                    {"log",(char *)sizeof(log_t)},
                    {"log_t",(char *)sizeof(log_t)},
                    {"log_buf_t",(char *)sizeof(log_buf_t)},
                    {"LOG_DEV_STATUS_SIZE",
                         (char *)LOG_DEV_STATUS_SIZE},
                    {"log_dev_status_t",(char *)sizeof(log_dev_status_t)},
                    {"log_seg_t",(char *)sizeof(log_seg_t)},
                    {"LOG_SPECIAL_IOV_MAX",
                         (char *)LOG_SPECIAL_IOV_MAX},
                    {"LOG_SPECIAL_SIZE",(char *)LOG_SPECIAL_SIZE},
                    {"log_special_t",(char *)sizeof(log_special_t)},
                    {"status",(char *)sizeof(log_status_t)},
                    {"log_status_t",(char *)sizeof(log_status_t)},
                    {"log_wrap_t",(char *)sizeof(log_wrap_t)},
                    {"long",(char *)sizeof(long)},
                    {"MAX_READ_LEN",(char *)MAX_READ_LEN},
                    {"mem_region_t",(char *)sizeof(mem_region_t)},
                    {"MIN_NV_RANGE_SIZE",(char *)MIN_NV_RANGE_SIZE},
                    {"MIN_FLUSH_BUF_LEN",(char *)MIN_FLUSH_BUF_LEN},
                    {"MIN_RECOVERY_BUF_LEN",(char *)MIN_RECOVERY_BUF_LEN},
                    {"MIN_TRANS_SIZE",(char *)MIN_TRANS_SIZE},
                    {"mutex",(char *)sizeof(RVM_MUTEX)},
                    {"NUM_CACHE_TYPES",(char *)NUM_CACHE_TYPES},
                    {"NV_LOCAL_MAX",(char *)NV_LOCAL_MAX},
                    {"nv",(char *)sizeof(nv_range_t)},
                    {"nv_range_t",(char *)sizeof(nv_range_t)},
                    {"NV_RANGE_OVERHEAD",(char *)NV_RANGE_OVERHEAD},
                    {"page_size",(char *)PAGE_KEY}, /* not a type */
                    {"range_t",(char *)sizeof(range_t)},
                    {"rec_end_t",(char *)sizeof(rec_end_t)},
                    {"rec_hdr_t",(char *)sizeof(rec_hdr_t)},
                    {"RECOVERY_BUF_LEN",(char *)RECOVERY_BUF_LEN},
                    {"region_t",(char *)sizeof(region_t)},
                    {"rvm_length_t",(char *)sizeof(rvm_length_t)},
                    {"rvm_offset_t",(char *)sizeof(rvm_offset_t)},
                    {"rvm_options_t",(char *)sizeof(rvm_options_t)},
                    {"rvm_region_t",(char *)sizeof(rvm_region_t)},
                    {"rvm_tid_t",(char *)sizeof(rvm_tid_t)},
                    {"rw_lock",(char *)sizeof(rw_lock_t)},
                    {"rw_lock_t",(char *)sizeof(rw_lock_t)},
                    {"rw_lock_mode_t",(char *)sizeof(rw_lock_mode_t)},
                    {"seg_t",(char *)sizeof(seg_t)},
                    {"struct_id_t",(char *)sizeof(struct_id_t)},
                    {"int_tid_t",(char *)sizeof(int_tid_t)},
                    {"timeval",(char *)sizeof(struct timeval)},
                    {"trans_hdr_t",(char *)sizeof(trans_hdr_t)},
                    {"TRANS_SIZE",(char *)TRANS_SIZE},
                    {"tree_links_t",(char *)sizeof(tree_links_t)},
                    {"tree_node_t",(char *)sizeof(tree_node_t)},
                    {"TRUNCATE",(char *)TRUNCATE},
                    {"ulong",(char *)sizeof(unsigned long)},
                    {"unsigned",(char *)sizeof(unsigned)},
                    {"",NULL}           /* end mark, do not delete */
                    };
/* sizeof command support */
static rvm_bool_t do_sizeof()
    {
    char            type_name[STR_NAME_LEN+1]; /* name buffer */
    long            type_index;         /* index of type in type vector */
    rvm_bool_t      retval = rvm_true;

    DO_FOREVER
        {
        /* scan and lookup type name */
        skip_white(&cmd_cur);
        if (scan_stop(*cmd_cur)) return retval; /* all done ?? */
        (void)scan_str(type_name,STR_NAME_LEN);
        type_index = lookup_str_name(type_name,type_vec,"type/struct");
        if (type_index == (long)UNKNOWN)
            return rvm_false;

        /* print the size */
        switch ((long)type_vec[type_index].target)
            {                           /* interpret request */
          case ALL_KEY:
            for (type_index=0;rvm_true;type_index++)
                {
                if (type_vec[type_index].str_name[0] == '\0') break;
                if (chk_sigint(stdout)) break;

                switch ((key_id_t)type_vec[type_index].target)
                    {
                  case ALL_KEY: break;
                  case PAGE_KEY:
                    printf("  page size = %ld\n",page_size); break;
                  default:
                    /* eliminate special abbreviations */
                    if (strstr(type_vec[type_index+1].str_name,
                               type_vec[type_index].str_name) ==
                        type_vec[type_index+1].str_name) break;
                    /* print */
                    printf("  sizeof(%s) = %ld\n",
                           type_vec[type_index].str_name,
                           (long)type_vec[type_index].target);
                    }
                }
            break;
          case PAGE_KEY:
            printf("  page size = %ld\n",page_size);
            break;

          default:
            printf("  sizeof(%s) = %ld\n",type_vec[type_index].str_name,
                   (long)type_vec[type_index].target);
            }
        }
    }
/* peek/poke file & buffer management */
static void init_peekpoke_buf()
    {

    peekpoke.r_length = 0;
    peekpoke.ptr = 0;
    RVM_ZERO_OFFSET(peekpoke.offset);
    RVM_ZERO_OFFSET(peekpoke.end_offset);
    RVM_ZERO_OFFSET(peekpoke.sticky_offset);

    }

/* close peek/poke file */
static void close_peekpoke_dev()
    {

    close_dev(&peekpoke.dev);
    if (peekpoke.dev.name != NULL)
        free(peekpoke.dev.name);
    peekpoke.dev.name = NULL;
    peekpoke.dev.raw_io = rvm_false;
    init_peekpoke_buf();

    }
/* open peek/poke file */
static rvm_bool_t open_peekpoke_dev()
    {
    char            *file_name;         /* temporary file name ptr */
    rvm_length_t    temp;

    /* scan file name if specified */
    skip_white(&cmd_cur);
    if (!scan_stop(*cmd_cur))
        {
        if (!isdigit(*cmd_cur))
            {
            file_name = malloc(MAXPATHLEN);
            if (file_name == NULL)
                {
                fprintf(stderr,"\n? Heap exhausted.\n");
                return rvm_false;
                }
            (void)scan_str(file_name,MAXPATHLEN);

            /* see if it's same as already in use */
            if (peekpoke.dev.handle != 0) {
                if (strcmp(peekpoke.dev.name,file_name))
                    close_peekpoke_dev();       /* no, must close old */
                else
                    {
                    free(file_name);    /* yes, scrap name */
                    init_peekpoke_buf(); /* force buffer refill */
                    }
	    }

            /* recheck to see if must open new file */
            if (peekpoke.dev.handle == 0)
                {
                peekpoke.dev.name = file_name;
                if (open_dev(&peekpoke.dev,O_RDWR,0) < 0)
                    {
                    pr_rvm_error(stderr,RVM_EIO,"opening peek/poke file");
                    close_peekpoke_dev();
                    return rvm_false;
                    }

                /* get device characteristics */
                if (set_dev_char(&peekpoke.dev,NULL) != 0)
                    {
                    pr_rvm_error(stderr,RVM_EIO,
                                 "getting peek/poke file length");
                    close_peekpoke_dev();
                    return rvm_false;
                    }
                /* if raw i/o device, must get max length */
                if (peekpoke.dev.raw_io)
                    {
                    /* temporary -- until segments get labels */
                    printf("\n  Please enter length of partition: ");
                    scanf("%lu",&temp); skip_lf();
                    peekpoke.dev.num_bytes=RVM_LENGTH_TO_OFFSET(temp);
                    }
                init_peekpoke_buf();
                }
            }
        }

    /* be sure we have a file */
    if (peekpoke.dev.handle == 0)
        {
        fprintf(stderr,"\n? No peek/poke file is open.\n");
        return rvm_false;
        }

    return rvm_true;
    }
/* write contents of peekpoke buffer to device */
static rvm_bool_t write_peekpoke(err_stream)
    FILE            *err_stream;        /* error output stream */
    {
    long            length;

    length = write_dev(&peekpoke.dev,&peekpoke.offset,peekpoke.buf,
                       peekpoke.r_length,SYNCH);
    if (length < 0)
        {
        pr_rvm_error(err_stream,RVM_EIO,"writing peek/poke file");
        return rvm_false;
        }
    assert(length == peekpoke.r_length);

    return rvm_true;
    }
/* fill peekpoke buffer */
static rvm_bool_t read_peekpoke(offset,length,rewrite,err_stream)
    rvm_offset_t    *offset;            /* read offset */
    long            length;             /* minimum read length */
    rvm_bool_t      rewrite;            /* write contents back if true */
    FILE            *err_stream;        /* error output stream */
    {
    rvm_offset_t    end_offset;
    rvm_length_t    read_length;

    /* see if request is in buffer */
    end_offset = RVM_ADD_LENGTH_TO_OFFSET(*offset,length);
    if (RVM_OFFSET_GTR(end_offset,peekpoke.end_offset)
        || RVM_OFFSET_LSS(*offset,peekpoke.offset))
        {
        /* no, see if must write buffer back to device before refill */
        if (rewrite && (peekpoke.r_length != 0))
            if (!write_peekpoke(err_stream)) return rvm_false;

        /* reload with requested region */
        peekpoke.offset = CHOP_OFFSET_TO_SECTOR_SIZE(*offset);
        end_offset = ROUND_OFFSET_TO_SECTOR_SIZE(end_offset);
        if (RVM_OFFSET_GTR(end_offset,peekpoke.dev.num_bytes))
            {
            fprintf(err_stream,"\n Offset is beyond end of file\n");
            fprintf(err_stream," -- setting read length to end of file\n");
            end_offset = peekpoke.dev.num_bytes;
            }
        read_length = RVM_OFFSET_TO_LENGTH(
                        RVM_SUB_OFFSETS(end_offset,peekpoke.offset));

        /* check request length */
        if (read_length > peekpoke.length)
            {
            fprintf(err_stream,
                    "\n Read request too long for peek/poke buffer\n");
            fprintf(err_stream," -- setting length to buffer size\n");
            read_length = peekpoke.length;
            }
        /* do the read */
        peekpoke.r_length = read_dev(&peekpoke.dev,&peekpoke.offset,
                                     peekpoke.buf,read_length);
        if (peekpoke.r_length < 0)
            {
            pr_rvm_error(err_stream,RVM_EIO,"reading peek/poke file");
            return rvm_false;
            }
        peekpoke.end_offset = 
            RVM_ADD_LENGTH_TO_OFFSET(peekpoke.offset,peekpoke.r_length);
        }

    /* set ptr to requested byte */
    peekpoke.ptr = RVM_OFFSET_TO_LENGTH(
                      RVM_SUB_OFFSETS(*offset,peekpoke.offset));
    return rvm_true;
    }

/* peekpoke buffer check */
static char *chk_peekpoke(offset,length,err_stream)
    rvm_offset_t    *offset;            /* initial offset in file */
    rvm_length_t    length;             /* data length needed */
    FILE            *err_stream;        /* error output stream */
    {

    if (!read_peekpoke(offset,length,rvm_false,err_stream))
        return NULL;

    return &peekpoke.buf[peekpoke.ptr];
    }
/* peek cmd support */
static rvm_bool_t do_peek()
    {
    FILE            *out_stream;        /* output stream */
    FILE            *err_stream;        /* error output stream */
    rvm_offset_t    offset;             /* initial offset in file */
    rvm_offset_t    pr_offset;          /* printing copy of offset (pr_range) */
    int             base=0;             /* radix of offset */
    rvm_bool_t      no_err = rvm_true;

    /* see if redirection wanted */
    get_redirect(&out_stream,&err_stream);

    /* check file state */
    if (!open_peekpoke_dev())
        goto err_exit;

    /* see if we have a repeat line */
    skip_white(&cmd_cur);
    if (scan_stop(*cmd_cur) && (!is_sticky_line))
        is_sticky_line = rvm_true;  /* process empty line like sticky repeat */

    /* print loop */
    DO_FOREVER
        {
        /* check for end of command */
        if (chk_sigint(out_stream)) goto exit;
        skip_white(&cmd_cur);
        if ((*cmd_cur == '\0') || (*cmd_cur == '>')
            || (*cmd_cur == '\n') || is_sticky_line)
            {
            /* see if we're done */
            if (!is_sticky_line) goto exit; /* yes */
            offset = peekpoke.sticky_offset;
            is_sticky_line = rvm_false;
            *cmd_cur = '\0';
            }
        else
            {
            if (is_float(cmd_cur,NULL) == 1)
                {
                fprintf(stderr,
                        "\n? Offset may not be floating point\n");
                goto err_exit;
                }
            /* scan file starting offset, print type, format, and count */
            offset = str2off(cmd_cur,&cmd_cur,&base);
            skip_white(&cmd_cur);
            if (*cmd_cur == '/')
                {
                if (!scan_num_format())
                    {
                    bad_num_format(err_stream);
                    goto err_exit;
                    }
                }
            else
                {
                /* check for garbage */
                if (!scan_stop(*cmd_cur) && (*cmd_cur != ' ')
                     && (!isdigit(*cmd_cur)))
                    {
                    fprintf(err_stream,"?  Unknown character: %c\n",
                            *cmd_cur);
                    goto err_exit;
                    }
                init_num_format();
                }
            }

        /* print requested region */
        pr_offset = offset;             /* make copy for pr_range */
        if (!pr_data_range(out_stream,err_stream,0,LINE_WIDTH,
                           &offset,&pr_offset,base,num_count,
                           chk_peekpoke,&peekpoke.dev.num_bytes,
                           EOF_STR))
            goto err_exit;
        }

err_exit: no_err = rvm_false;
exit: close_redirect(out_stream,err_stream);
    if (no_err) peekpoke.sticky_offset = offset;
    return no_err;
    }
/* scan poke value */
static rvm_bool_t scan_poke_val(offset,scanned_str,scanned_float,
                          dbl_float,err_stream)
    rvm_offset_t    *offset;
    rvm_bool_t      *scanned_str;
    rvm_bool_t      *scanned_float;
    double          *dbl_float;
    FILE            *err_stream;
    {
    long            size;

    skip_white(&cmd_cur);
    *scanned_float = rvm_false;
    *scanned_str = rvm_false;

    if (*cmd_cur == '\"')
        {                   /* scan quoted string */
        size = scan_qstr(temp_str,CMD_MAX);
        if (size < 0)
            {
            fprintf(err_stream,
                    "\n? Quoted string not terminated with '\"'\n");
            return rvm_false;
            }
        *scanned_str = rvm_true;
        }
    else                        /* scan numeric data */
        switch (is_float(cmd_cur,NULL))
            {
          case 1:               /* scan float */
            *dbl_float = str2dbl(cmd_cur,&cmd_cur);
            *scanned_float = rvm_true;
            break;
          case 0:
            if (num_format & ((long)float_sw | (long)double_sw))
                {
                *dbl_float = str2dbl(cmd_cur,&cmd_cur);
                *scanned_float = rvm_true;
                break;
                }
            *offset = str2off(cmd_cur,&cmd_cur,NULL);
            break;
          default:              /* report error */
            fprintf(err_stream,"\n? Unknown data type begining with: ");
            pr_char(err_stream,*cmd_cur,NULL);
            putc('\n',err_stream);
            return rvm_false;
            }

    return rvm_true;
    }
/* replicate poke assignments */
static rvm_bool_t repeat_poke(offset,init_offset)
    rvm_offset_t    *offset;             /* current offset in file */
    rvm_offset_t    *init_offset;        /* initial offset in file */
    {
    int             i;
    char            *copy_buf = NULL;   /* repeated assignment buffer */
    rvm_length_t    repeat_len;         /* length of copy_buf */
    rvm_bool_t      no_err = rvm_true;

    /* repeat current assignments if necessary */
    if (--num_count > 0)
        {
        /* copy the original assignments */
        repeat_len = RVM_OFFSET_TO_LENGTH(
                         RVM_SUB_OFFSETS(*offset,*init_offset));
        if (repeat_len == 0) return rvm_true; /* nothing to do */
        copy_buf = malloc(repeat_len);
        if (copy_buf == NULL)
            {
            fprintf(stderr,"\n? Heap exhausted\n");
            return rvm_false;
            }
        if (!read_peekpoke(init_offset,repeat_len,
                           rvm_true,stderr))
            goto err_exit;
        BCOPY(&peekpoke.buf[peekpoke.ptr],copy_buf,
              repeat_len);

        /* repeat the assignments */
        for (i=num_count; i > 0; i--)
            {
            if (RVM_OFFSET_GTR(
                    RVM_ADD_LENGTH_TO_OFFSET(*offset,repeat_len),
                    peekpoke.dev.num_bytes))
                {
                fprintf(stderr,"\n ?Repeated assignment past end of file\n");
                fprintf(stderr,"  -- stopping at file end\n");
                repeat_len = RVM_OFFSET_TO_LENGTH(RVM_SUB_OFFSETS(
                                 peekpoke.dev.num_bytes,*offset));
                i = 0;
                }
            if (!read_peekpoke(offset,repeat_len,
                               rvm_true,stderr))
                goto err_exit;
            BCOPY(copy_buf,&peekpoke.buf[peekpoke.ptr],
                  repeat_len);
            *offset = RVM_ADD_LENGTH_TO_OFFSET(*offset,
                                              repeat_len);
            }
        }
    goto exit;

err_exit:
    no_err = rvm_false;
exit:
    num_count = 0;
    if (copy_buf != NULL) free(copy_buf);
    return no_err;
    }
/* poke cmd support */
static rvm_bool_t do_poke()
    {
    rvm_offset_t    init_offset;        /* initial offset in file */
    rvm_offset_t    offset;             /* current offset in file */
    rvm_offset_t    temp;               /* temporary buffer for all integers */
    rvm_length_t    size=0;             /* size of mem item to assign */
    rvm_length_t    one_word;           /* word-sized temp */
    double          dbl_float;          /* temporary buffer for float values */
    float           sngl_float;         /* temporary buffer for float values */
    char            *data_src;          /* ptr to assignment data */
    int             num_offsets = 0;    /* number of offsets scanned */
    rvm_bool_t      scanned_str;
    rvm_bool_t      scanned_float;

    /* check file state */
    if (!open_peekpoke_dev())
        return rvm_false;
    init_num_format();

    /* modification loop */
    DO_FOREVER
        {
        skip_white(&cmd_cur);
        if (scan_stop(*cmd_cur))
            goto exit;                  /* all done */

        if (!scan_poke_val(&temp,&scanned_str,&scanned_float,
                           &dbl_float,stderr))
            return rvm_false;

        /* see if just scanned a new offset or end of line*/
        skip_white(&cmd_cur);
        if ((*cmd_cur == '/') || (*cmd_cur == '='))
            {
            if (scanned_float || scanned_str)
                {
                fprintf(stderr,
                        "\n? Offset may not be float or string\n");
                return rvm_false;
                }

            /* repeat assignments & set new offset */
            if (num_offsets > 0)
                if (!repeat_poke(&offset,&init_offset))
                    return rvm_false;
            offset = temp;
            init_offset = offset;
            num_offsets++;
            /* scan count & format if specified */
            if (*cmd_cur == '/')
                {
                if (!scan_num_format())
                    {
                    bad_num_format(stderr);
                    return rvm_false;
                    }
                }
            else
                if (num_format == 0)
                    init_num_format();
                else num_count = 1;

            /* scan assignment operator */
            skip_white(&cmd_cur);
            if (*cmd_cur != '=')
                {
                fprintf(stderr,"\n? Offset must be followed by '='\n");
                return rvm_false;
                }
            incr_cur(1);
            continue;                   /* repeat scan for data */
            }
        /* set size  and data source for assignment */
        if (scanned_str)
            {
            data_src = temp_str;
            size = strlen(temp_str) + 1; /* copy null, too */
            }
        else
            {
            /* make numeric available as both float and offset */
            if (scanned_float)
                BCOPY(&dbl_float,&temp,sizeof(rvm_offset_t));
            else dbl_float = RVM_OFFSET_TO_LENGTH(temp);
            one_word = RVM_OFFSET_TO_LENGTH(temp);
            data_src = (char *)&one_word;

            /* do type-specific sizing */
            switch (num_format & ALL_SIZES)
                {
              case char_sw:
              case byte_sw:
                size = sizeof(char);
                break;
              case short_sw:
                size = sizeof(short);
                break;
              case long_sw:
                size = sizeof(long);
                break;
              case offset_sw:
                size = sizeof(rvm_offset_t);
                data_src = (char *)&temp;
                break;
              case double_sw:
                size = sizeof(double);
                data_src = (char *)&dbl_float;
                break;
              case float_sw:
                size = sizeof(float);
                sngl_float = dbl_float;
                data_src = (char *)&sngl_float;
                break;
              default:          assert(rvm_false);
                }
            }
        /* check alignment & do assignment */
        if (RVM_OFFSET_GTR(RVM_ADD_LENGTH_TO_OFFSET(offset,size),
                           peekpoke.dev.num_bytes))
            {
            fprintf(stderr,"\n ?Assignment past end of file\n");
            goto exit;
            }
        if (!read_peekpoke(&offset,size,rvm_true,stderr))
            return rvm_false;
        if (!num_format_align(num_format,&peekpoke.buf[peekpoke.ptr]))
            {
            bad_format_align(stderr,num_format);
            write_peekpoke(stderr);     /* preserve previous valid assignments */
            return rvm_false;
            }
        (void)BCOPY(data_src,&peekpoke.buf[peekpoke.ptr],size);
        offset = RVM_ADD_LENGTH_TO_OFFSET(offset,size);
        }

    /* repeat last assignments; do final write-back to peek/poke file */
exit:
    if (num_offsets > 0)
        if (!repeat_poke(&offset,&init_offset))
            return rvm_false;
    return write_peekpoke(stderr);
    }
/* sub record size determination; -1 if no valid header */
static rvm_length_t sub_rec_size(rec_hdr)
    rec_hdr_t       *rec_hdr;           /* generic record header */
    {
    rvm_length_t    size;

    switch (rec_hdr->struct_id)
        {
      case trans_hdr_id:
        size = sizeof(trans_hdr_t);
        break;
      case log_seg_id: case nv_range_id:
        size = rec_hdr->rec_length;
        break;
      case rec_end_id:
        size = sizeof(rec_end_t);
        break;
      case log_wrap_id:
        size = sizeof(log_wrap_t);
        break;
      default:
        size = -1;
        }

    return size;
    }
/* log record type name printer */
static void print_hdr_type(out_stream,rec_hdr)
    FILE            *out_stream;        /* output stream */
    rec_hdr_t       *rec_hdr;           /* generic record header */
    {
    switch (rec_hdr->struct_id)
        {
      case trans_hdr_id:
        fprintf(out_stream,"TRANSACTION"); return;

      case log_seg_id:
        fprintf(out_stream,"SEGMENT DICTIONARY ENTRY"); return;

      case log_wrap_id:
        fprintf(out_stream,"WRAP-AROUND MARKER"); return;

      default:
	fprintf(out_stream,"UNKNOWN RECORD TYPE, id = %u",
	        (unsigned)rec_hdr->struct_id);
        }
    }    

#ifdef UNUSED_FUNCTIONS
/* print record summary */
static void print_rec_summary(out_stream,rec_hdr,cur_msg,direction,print)
    FILE            *out_stream;        /* output stream */
    rec_hdr_t       *rec_hdr;           /* generic record header */
    char            *cur_msg;           /* currency message */
    rvm_bool_t       direction;
    rvm_bool_t       print;
{
    if (print == rvm_true) {
	fprintf(out_stream,
		"Record number: %7.1lu  Offset: ",rec_hdr->rec_num);
	prc_offset(out_stream,&cur_offset,10,NULL);
	fprintf(out_stream,"  Type: ");
	print_hdr_type(out_stream,rec_hdr);
	fprintf(out_stream,"\n");
    }
    if (!chk_hdr_sequence(log,rec_hdr,direction))
	fprintf(out_stream,"** Record number is out of sequence **\n");
}
#endif
/* record header printer */
static void print_rec_hdr(out_stream,rec_hdr,cur_msg)
    FILE            *out_stream;        /* output stream */
    rec_hdr_t       *rec_hdr;           /* generic record header */
    char            *cur_msg;           /* currency message */
    {
    fprintf(out_stream,
            "Record number: %7.1lu     Type: ",rec_hdr->rec_num);
    print_hdr_type(out_stream,rec_hdr);
    if (!chk_hdr_sequence(log,rec_hdr))
        fprintf(out_stream,"\n ** Record number is out of sequence **");
    fprintf(out_stream,"\n\n");

    fprintf(out_stream,"  Timestamp:                 ");
    pr_timeval(out_stream,&rec_hdr->timestamp,rvm_true,NULL);
    if (cur_msg != NULL)
        fprintf(out_stream," %s",cur_msg);
    else
        if (!chk_hdr_currency(log,rec_hdr))
            fprintf(out_stream," (old)");
    putc('\n',out_stream);

    fprintf(out_stream,"  Record length:   %11.1lu",rec_hdr->rec_length);
    fprintf(out_stream,"   Log offset:");
    prc_offset(out_stream,&cur_offset,10,NULL);
    putc('\n',out_stream);
    }
/* record end marker printer */
static void print_rec_end(out_stream,rec_end)
    FILE            *out_stream;        /* output stream */
    rec_end_t       *rec_end;           /* record end marker */
    {
    rvm_offset_t    log_offset;

    log_offset = RVM_ADD_LENGTH_TO_OFFSET(log_buf->offset,
                                          log_buf->ptr);
    fprintf(out_stream,"\n  End of record number:%7.1lu   Log Offset:",
            rec_end->rec_hdr.rec_num);
    prc_offset(out_stream,&log_offset,10,NULL);
    putc('\n',out_stream);
    fprintf(out_stream,"    Record length: %11.1lu   Back link:  %5.1lu\n",
            rec_end->rec_hdr.rec_length,rec_end->sub_rec_len);

    }
/* modification range printer */
static void print_range(out_stream,nv)
    FILE            *out_stream;        /* output stream */
    nv_range_t      *nv;                /* range header */
    {
    rvm_offset_t    log_offset;

    log_offset = RVM_ADD_LENGTH_TO_OFFSET(log_buf->offset,
                                          log_buf->ptr);
    fprintf(out_stream,"  Modification range:  %7.1lu   Log offset:",
            nv->range_num);
    prc_offset(out_stream,&log_offset,10,NULL);
    putc('\n',out_stream);
    fprintf(out_stream,
            "    VM address:     %#10.1lx   Length:     %5.1lu\n",
            (long)nv->vmaddr,nv->length);
    fprintf(out_stream,
            "    Segment code:          %3.1lu   Offset:",
            nv->seg_code);
    prc_offset(out_stream,&nv->offset,10,NULL);
    putc('\n',out_stream);
    fprintf(out_stream,
            "    Record length:       %5.1lu   Back link:  %5.1lu\n",
            nv->rec_hdr.rec_length,nv->sub_rec_len);

    }
/* transaction header printer */
static void print_trans_hdr(out_stream,trans_hdr)
    FILE            *out_stream;        /* output stream */
    trans_hdr_t     *trans_hdr;         /* transaction header */
    {

    /* print transaction header, start/commit times */
    fprintf(out_stream,"  TID:              ");
    prw_timeval(out_stream, &trans_hdr->uname);
    putc('\n',out_stream);
    fprintf(out_stream,"  Trans start  ");
    if (TRANS_HDR(RESTORE_FLAG))
        fprintf(out_stream,"(restore):    ");
    else fprintf(out_stream,"(no_restore): ");
    pr_timeval(out_stream,&trans_hdr->uname,rvm_true,NULL);
    putc('\n',out_stream);
    fprintf(out_stream,"  Trans commit ");
    if (TRANS_HDR(FLUSH_FLAG))
        fprintf(out_stream,"(flush):      ");
    else fprintf(out_stream,"(no_flush):   ");
    pr_timeval(out_stream,&trans_hdr->commit_stamp,rvm_true,NULL);
    putc('\n',out_stream);
    fprintf(out_stream,"  Flags: ");
    if (TRANS_HDR(RVM_COALESCE_RANGES))
        fprintf(out_stream,"coalesce_ranges ");
    if (TRANS_HDR(RVM_COALESCE_TRANS))
        fprintf(out_stream,"coalesce_trans ");
    if (TRANS_HDR(FLUSH_MARK))
        fprintf(out_stream,"flush_mark ");
    putc('\n',out_stream);
    if (TRANS_HDR(RVM_COALESCE_TRANS))
        fprintf(out_stream,"  Trans coalesced:       %5ld\n",
                trans_hdr->n_coalesced);
    fprintf(out_stream,"  First record:          ");
    pr_bool(out_stream,((trans_hdr->flags & FIRST_ENTRY_FLAG) != 0)
            ,NULL);
    fprintf(out_stream,"      Last record:        ");
    pr_bool(out_stream,((trans_hdr->flags & LAST_ENTRY_FLAG) != 0)
            ,NULL);
    putc('\n',out_stream);

    fprintf(out_stream,"  Number ranges:      %8.1lu",
            trans_hdr->num_ranges);
    fprintf(out_stream,"   Tot. rec. length:%11.1lu\n\n",
            trans_hdr->rec_hdr.rec_length+sizeof(rec_end_t));
    }
/* transaction printer */
static void print_transaction(out_stream,err_stream,trans_hdr)
    FILE            *out_stream;        /* output stream */
    FILE            *err_stream;        /* error output stream */
    trans_hdr_t  *trans_hdr;         /* transaction header */
    {
    nv_range_t      *nv;                /* range header */
    rec_end_t       *rec_end;           /* record end marker */
    long            i;
    rvm_return_t    retval;

    /* print transaction header, start/commit times */
    print_trans_hdr(out_stream,trans_hdr);

    /* print ranges */
    log_buf->ptr += sizeof(trans_hdr_t);
    for (i=trans_hdr->num_ranges; i>0; i--)
        {
        /* check ^c */
        if (chk_sigint(out_stream)) return;

        /* scan to next range and print */
        if ((retval=scan_nv_forward(log,NO_SYNCH)) != RVM_SUCCESS)
            {
            pr_rvm_error(err_stream,retval,"while scanning log");
            return;
            }
        nv = (nv_range_t *)&log_buf->buf[log_buf->ptr];
        print_range(out_stream,nv);
        log_buf->ptr += nv->rec_hdr.rec_length;
        }

    /* print end marker */
        if ((retval=scan_nv_forward(log,NO_SYNCH)) != RVM_SUCCESS)
            {
            pr_rvm_error(err_stream,retval,"while scanning log");
            return;
            }
        rec_end = (rec_end_t *)&log_buf->buf[log_buf->ptr];
        print_rec_end(out_stream,rec_end);
    }
/* log segment dictionary entry printer */
static void print_log_seg(out_stream,rec_hdr)
    FILE            *out_stream;        /* output stream */
    rec_hdr_t       *rec_hdr;           /* log record header */
    {
    log_seg_t       *log_seg;           /* log segment dictionary entry */
    char            *name;              /* segment file name */

    log_seg = (log_seg_t *)RVM_ADD_LENGTH_TO_ADDR(rec_hdr,
                                                  sizeof(rec_hdr_t));
    name = RVM_ADD_LENGTH_TO_ADDR(rec_hdr,LOG_SPECIAL_SIZE);

    fprintf(out_stream,"  Segment code:        %7.1lu\n",
            log_seg->seg_code);
    fprintf(out_stream,"  Segment file:            %s\n",
            name);
    fprintf(out_stream,"  Segment length:   ");
    prc_offset(out_stream,&log_seg->num_bytes,10,NULL);
    putc('\n',out_stream);
    }
/* log record printer */
static void print_rec(out_stream,err_stream,rec_hdr,cur_msg)
    FILE            *out_stream;        /* output stream */
    FILE            *err_stream;        /* error output stream */
    rec_hdr_t       *rec_hdr;           /* generic record header */
    char            *cur_msg;           /* currency message */
    {

    /* print standard header data */
    print_rec_hdr(out_stream,rec_hdr,cur_msg);

    /* print type-specific data */
    switch (rec_hdr->struct_id)
        {
      case trans_hdr_id:
        print_transaction(out_stream,err_stream,
                          (trans_hdr_t *)rec_hdr);
        break;
      case log_seg_id:
        print_log_seg(out_stream,rec_hdr);
        break;
      case log_wrap_id:
        break;                          /* nothing more to print */
      default:      assert(rvm_false);  /* log damage if we get here */
        }

    putc('\n',out_stream);
    }
/* set current log position and record ptr */
static rec_hdr_t *set_log_position()
    {
    rec_hdr_t       *rec_hdr;           /* generic record header */
    trans_hdr_t  *trans_hdr;      /* transaction header */

    assert(log_buf->ptr != -1);
    no_rec = rvm_false;
    rec_hdr = (rec_hdr_t *)(&log_buf->buf[log_buf->ptr]);
    cur_rec_num = rec_hdr->rec_num;
    cur_offset = RVM_ADD_LENGTH_TO_OFFSET(log_buf->offset,
                                          log_buf->ptr);
    cur_timestamp = rec_hdr->timestamp;

    /* type-specific actions */
    if (rec_hdr->struct_id == trans_hdr_id)
        {
        trans_hdr = (trans_hdr_t *)rec_hdr;
        cur_trans_uname = trans_hdr->uname;
        cur_trans_commit = trans_hdr->commit_stamp;
        }

    return rec_hdr;
    }

/* current log record printer */
static void print_cur_rec(out_stream,err_stream)
    FILE            *out_stream;        /* output stream */
    FILE            *err_stream;        /* error output stream */
    {
    rec_hdr_t       *rec_hdr;           /* generic record header */

    /* set current log position and record ptr */
    rec_hdr = set_log_position();

    /* print record */
    print_rec(out_stream,err_stream,rec_hdr,NULL);
    }

#ifdef UNUSED_FUNCTIONS
/* Print a summary of the current log record: one line */
static void print_cur_summary(out_stream,err_stream,direction,print)
    FILE            *out_stream;        /* output stream */
    FILE            *err_stream;        /* error output stream */
    rvm_bool_t       direction;
    rvm_bool_t       print;
{
    rec_hdr_t       *rec_hdr;           /* generic record header */

    /* set current log position and record ptr */
    rec_hdr = set_log_position();

    /* print record */
    print_rec_summary(out_stream,rec_hdr,NULL,direction,print);
}
#endif
/* log sub record printer */
static void print_cur_sub_rec(out_stream,err_stream)
    FILE            *out_stream;        /* output stream */
    FILE            *err_stream;        /* error output stream */
    {
    rec_hdr_t       *rec_hdr;           /* generic record header */

    /* set current log position and record ptr */
    rec_hdr = set_log_position();

    /* print record */
    if ((rec_hdr->struct_id != nv_range_id)
        && (rec_hdr->struct_id != rec_end_id))
        print_rec_hdr(out_stream,rec_hdr,NULL);
    switch (rec_hdr->struct_id)
        {
      case trans_hdr_id:
        print_trans_hdr(out_stream,(trans_hdr_t *)rec_hdr);
        break;
      case log_seg_id:
        print_log_seg(out_stream,rec_hdr);
        break;
      case rec_end_id:
        print_rec_end(out_stream,(rec_end_t *)rec_hdr);
        break;
      case nv_range_id:
        print_range(out_stream,(nv_range_t *)rec_hdr);
        break;
      case log_wrap_id:
        break;
      default:
        log_buf->ptr = -1;
        no_rec = rvm_true;
        }

    }
/* log status header printer */
static void print_log_hdr(FILE *out_stream, FILE *err_stream)
{
    log_dev_status_t *log_dev_status = (log_dev_status_t *)status_io;

    fprintf(out_stream,"Status of log:\t%s\n\n",
            log_dev->name);
    fprintf(out_stream,"  log created on:\t");
    pr_timeval(out_stream,&status->status_init,rvm_true,NULL);
    putc('\n',out_stream);
    fprintf(out_stream,"  log created with:\t%s\n", log_dev_status->version);
    fprintf(out_stream,"\t\t\t%s\n", log_dev_status->log_version);
    fprintf(out_stream,"\t\t\t%s\n", log_dev_status->statistics_version);
    fprintf(out_stream,"  status last written:\t");
    pr_timeval(out_stream,&status->status_write,rvm_true,NULL);
    putc('\n',out_stream);
    fprintf(out_stream,"  last truncation:\t");
    pr_timeval(out_stream,&status->last_trunc,rvm_true,NULL);
    fprintf(out_stream,"\n\n");
}
/* log segment dictionary printer */
static void print_seg_dict(out_stream,err_stream,entry_num)
    FILE            *out_stream;        /* output stream */
    FILE            *err_stream;        /* error stream */
    int             entry_num;          /* index of entry */
    {
    device_t        *dev;

    if (log->seg_dict_vec[SEG_DICT_INDEX(entry_num)].seg != NULL)
        dev = &log->seg_dict_vec[SEG_DICT_INDEX(entry_num)].seg->dev;
    else
        dev = &log->seg_dict_vec[SEG_DICT_INDEX(entry_num)].dev;
    fprintf(out_stream,"  Segment code:        %7.1u\n",entry_num);
    fprintf(out_stream,"  Segment file:            %s\n",
            dev->name);
    fprintf(out_stream,"  Segment length:   ");
    prc_offset(out_stream,&(dev->num_bytes),10,NULL);
    putc('\n',out_stream);
    }
/* reset buffer if previous search failed */
static rvm_bool_t reset_buffer(new_direction)
    rvm_bool_t      new_direction;
    {
    rvm_return_t    retval;

    if (!no_rec) return rvm_true;

    if (cur_direction != new_direction) {
	cur_direction = (cur_direction == REVERSE) ? FORWARD : REVERSE;
    }

    if (RVM_OFFSET_EQL_ZERO(cur_offset))
        {
        fprintf(stdout,
                " -- no current record, use show head, tail, or");
        fprintf(stdout," record number\n");
        return rvm_false;
        }
    if ((retval=init_buffer(log,&cur_offset,cur_direction,NO_SYNCH))
        != RVM_SUCCESS)
        {
        pr_rvm_error(stderr,retval,"initializing buffer");
        return rvm_false;
        }

    return rvm_true;
    }
static rvm_return_t load_sub_rec(incr,direction)
    long            incr;
    rvm_bool_t      direction;
    {
    rec_hdr_t       *rec_hdr;
    rvm_offset_t    offset;             /* offset temp. */
    rvm_offset_t    target;
    long            tmp_ptr;
    rvm_return_t    retval = RVM_SUCCESS; /* return code */

    if (direction == REVERSE) incr = -incr;
    tmp_ptr = log_buf->ptr + incr;

    /* see if target in buffer */
    if ((tmp_ptr >= log_buf->length) || (tmp_ptr < 0))
        {                               /* no, must re-init buffer */
        if (tmp_ptr >= 0)
            offset = RVM_ADD_LENGTH_TO_OFFSET(log_buf->offset,tmp_ptr);
        else
            offset = RVM_SUB_LENGTH_FROM_OFFSET(log_buf->offset,
                                                -tmp_ptr);
        if (RVM_OFFSET_LSS(offset,log->status.log_start)
            || RVM_OFFSET_GEQ(offset,log->dev.num_bytes))
            goto no_record;             /* not in log */
        if (direction == REVERSE)
            offset = RVM_ADD_LENGTH_TO_OFFSET(offset,MAX_HDR_SIZE);
        if ((retval=init_buffer(log,&offset,direction,NO_SYNCH))
            != RVM_SUCCESS)
            goto no_record;
        if (direction == REVERSE)
            log_buf->ptr -= MAX_HDR_SIZE;
        }
    else
        log_buf->ptr = tmp_ptr;
    /* get as much of sub record as will fit in buffer */
    rec_hdr = (rec_hdr_t *)(&log_buf->buf[log_buf->ptr]);
    if ((tmp_ptr= sub_rec_size(rec_hdr)) == -1)
        goto no_record;
    if (tmp_ptr > (log_buf->length-MAX_HDR_SIZE))
        tmp_ptr = log_buf->length-MAX_HDR_SIZE;
    tmp_ptr += log_buf->ptr;
    if (tmp_ptr > log_buf->length)
        {
        offset = RVM_ADD_LENGTH_TO_OFFSET(log_buf->offset,tmp_ptr);
        if (RVM_OFFSET_LSS(offset,log->status.log_start)
            || RVM_OFFSET_GEQ(offset,log->dev.num_bytes))
            goto no_record;             /* not in log */
        target = RVM_ADD_LENGTH_TO_OFFSET(log_buf->offset,
                                          log_buf->ptr);
        if (direction == FORWARD)
            offset = target;
        if ((retval=init_buffer(log,&offset,direction,NO_SYNCH))
            != RVM_SUCCESS) goto no_record;
        if (direction == REVERSE)
            log_buf->ptr = RVM_OFFSET_TO_LENGTH(RVM_SUB_OFFSETS(
                                            target,log_buf->offset));
        }

    (void)set_log_position();
    return RVM_SUCCESS;

no_record:
    log_buf->ptr = -1;
    no_rec = rvm_true;
    return retval;
    }
/* load record header if at rec_end */
static rvm_bool_t load_rec_hdr(err_stream)
    FILE            *err_stream;        /* error output stream */
    {
    rec_end_t       *rec_end;           /* record end marker */
    long            tmp_ptr;            /* temporary buffer index */
    rvm_offset_t    offset;             /* disk offset temporary */
    rvm_return_t    retval;

    /* see if record header is in buffer */
    rec_end = (rec_end_t *)&log_buf->buf[log_buf->ptr];
    if (rec_end->rec_hdr.struct_id != log_wrap_id)
        {
        assert(rec_end->rec_hdr.struct_id == rec_end_id);
        tmp_ptr = log_buf->ptr - rec_end->rec_hdr.rec_length;
        if (tmp_ptr < 0)
            {
            /* must load record header */
            offset = RVM_SUB_LENGTH_FROM_OFFSET(log_buf->offset,
                                              -tmp_ptr);
            if ((retval=init_buffer(log,&offset,FORWARD,
                                    NO_SYNCH)) != RVM_SUCCESS)
                {
                no_rec = rvm_true;
                pr_rvm_error(err_stream,retval,"reading log");
                return rvm_false;
                }
            }
        else
            log_buf->ptr = tmp_ptr;
        }

    (void)set_log_position();
    return rvm_true;
    }
/* search for earliest intact record by searching backward from
     present log head */
static rvm_bool_t locate_earliest(out_stream,err_stream)
    FILE            *out_stream;        /* output stream */
    FILE            *err_stream;        /* error output stream */
    {
    rvm_offset_t    offset;
    rec_end_t       *rec_end;
    rvm_length_t    rec_cnt = 0;        /* record count */
    rvm_return_t    retval;

    /* init search at log head, reverse direction */
    cur_direction = REVERSE;
    cur_offset = status->log_head;
    if ((retval=init_buffer(log,&cur_offset,cur_direction,NO_SYNCH))
        != RVM_SUCCESS)
        {
        pr_rvm_error(err_stream,retval,INIT_BUF_STR);
        return rvm_false;
        }

    /* scan to the last intact record */
    DO_FOREVER
        {
        if (chk_sigint(out_stream)) break;
        if ((retval=scan_reverse(log,NO_SYNCH)) != RVM_SUCCESS)
            {
            pr_rvm_error(err_stream,retval,LOG_SCAN_STR);
            return rvm_false;
            }
        if (log_buf->ptr < 0)           /* see if good record */
            break;                      /* no, earliest found */

        /* see if found tail */
        offset = RVM_ADD_LENGTH_TO_OFFSET(log_buf->offset,
                                              log_buf->ptr);
        rec_end = (rec_end_t *)(&log_buf->buf[log_buf->ptr]);
        if (rec_end->rec_hdr.struct_id == rec_end_id)
            {
            offset = RVM_ADD_LENGTH_TO_OFFSET(offset,
                                              rec_end->rec_hdr.rec_length);
            if (RVM_OFFSET_EQL(offset,status->log_tail))
                break;                  /* yes, earliest found */
            }
        rec_cnt++;
        cur_offset = RVM_ADD_LENGTH_TO_OFFSET(log_buf->offset,
                                              log_buf->ptr);
        }
    /* get last good record end marker back in buffer */
    if (log_buf->ptr == -1)
        {
        if (rec_cnt == 0)
            {
            no_rec = rvm_true;          /* no good records found */
            return rvm_true;
            }
        if ((retval=init_buffer(log,&cur_offset,cur_direction,
                                NO_SYNCH))
            != RVM_SUCCESS)
            {
            pr_rvm_error(err_stream,retval,"initializing buffer");
            return rvm_false;
            }
        }
    /* re-init buffer with earliest record offset */
    rec_end = (rec_end_t *)(&log_buf->buf[log_buf->ptr]);
    if (rec_end->rec_hdr.struct_id != log_wrap_id)
        cur_offset = RVM_SUB_LENGTH_FROM_OFFSET(cur_offset,
                                              rec_end->rec_hdr.rec_length);
    cur_direction = FORWARD;            /* assume printing is next */
    if ((retval=init_buffer(log,&cur_offset,cur_direction,NO_SYNCH))
        != RVM_SUCCESS)
        {
        pr_rvm_error(err_stream,retval,"initializing buffer");
        return rvm_false;
        }
    (void)set_log_position();
    return rvm_true;
    }
static rvm_return_t locate_rec(rec_num,ts)
    long            rec_num;            /* record number if != 0 */
    struct timeval  *ts;                /* timestamp if != NULL */
    {
    rec_hdr_t       *rec_hdr;           /* generic record header */
    rvm_offset_t    offset;             /* offset temp. */
    rvm_bool_t      direction = REVERSE; /* search direction */
    rvm_return_t    retval = RVM_SUCCESS; /* return code */

    /* see which way to search */
    assert(!((rec_num == 0) && (ts == NULL)));
    if (no_rec)                         /* start at tail if no cur record */
        {
        if ((retval=init_buffer(log,&log->status.log_tail,
                                REVERSE,NO_SYNCH)) != RVM_SUCCESS)
            return retval;
        if ((retval=validate_rec_reverse(log,NO_SYNCH)) != RVM_SUCCESS)
            return retval;
        if (log_buf->ptr == -1)         /* tail record invalid */
            goto no_record;
        (void)set_log_position();
        }

    /* set direction of search */
    if (((rec_num != 0) && (rec_num > cur_rec_num)) ||
        ((ts != NULL) && (TIME_GTR(*ts,cur_timestamp))))
        direction = FORWARD;
    cur_direction = direction;

    /* search until match or no more records */
    while (((rec_num != 0) && (rec_num != cur_rec_num)) ||
           ((ts != NULL) && (!TIME_EQL(*ts,cur_timestamp))))
        {
        /* scan next record and setup record ptr */
        if (direction == FORWARD)
            {
            if ((retval=scan_forward(log,NO_SYNCH)) != RVM_SUCCESS)
                break;
            if (log_buf->ptr == -1) break; /* no more records */
            }
        else
            {
            if ((retval=scan_reverse(log,NO_SYNCH)) != RVM_SUCCESS)
                break;
            if (log_buf->ptr == -1) break;  /* no more records */
            /* be sure header of record is in buffer */
            rec_hdr = (rec_hdr_t *)(&log_buf->buf[log_buf->ptr]);

	    if (rec_hdr->struct_id == log_wrap_id) {
				/* scan_reverse may have just seen a
				   wrap marker and pointing right to it.
				   We need to read in the previous record
				   before we go furhter -- Clement */
		retval = validate_rec_reverse(log,NO_SYNCH);
		assert(retval == RVM_SUCCESS);
		if (log_buf->ptr == -1) break; /* no more records */
		rec_hdr = (rec_hdr_t *)(&log_buf->buf[log_buf->ptr]);
	    };

            log_buf->ptr -= rec_hdr->rec_length;
            if (log_buf->ptr < 0)
                {
                /* get the rest of the record */
                log_buf->ptr = -log_buf->ptr;
                offset = RVM_SUB_LENGTH_FROM_OFFSET(log_buf->offset,
                                                    log_buf->ptr);
                offset = RVM_ADD_LENGTH_TO_OFFSET(offset,
                                                  sizeof(nv_range_t));
                if ((retval=init_buffer(log,&offset,REVERSE,NO_SYNCH))
                    != RVM_SUCCESS) break;
                log_buf->ptr -= sizeof(nv_range_t);
                }
            }

        /* set current log position */
        (void)set_log_position();
        if (chk_sigint(NULL)) break;
        }

no_record:
    if ((log_buf->ptr == -1) || (retval != RVM_SUCCESS))
        no_rec = rvm_true;                  /* no current record */
    return retval;
    }
static rvm_return_t locate_next_sub_rec()
    {
    rec_hdr_t       *rec_hdr;           /* generic record header */
    long            incr;
    rvm_return_t    retval = RVM_SUCCESS; /* return code */

    /* get to known place */
    if (log_buf->ptr == -1)
        if (!reset_buffer(FORWARD))
            goto no_record;

    /* get record in buffer */
    rec_hdr = (rec_hdr_t *)(&log_buf->buf[log_buf->ptr]);
    switch (rec_hdr->struct_id)
        {
      case trans_hdr_id: 
      case rec_end_id:
      case log_seg_id: 
      case nv_range_id:
        incr = (long)sub_rec_size(rec_hdr);
        break;
      case log_wrap_id:
        if ((retval=init_buffer(log,&log->status.log_start,
                                FORWARD,NO_SYNCH)) == RVM_SUCCESS)
            return RVM_SUCCESS;
      default:
        goto no_record;
        }

    /* set log buffer ptr to next sub record */
    if ((retval=load_sub_rec(incr,FORWARD)) == RVM_SUCCESS)
        return RVM_SUCCESS;

no_record:
    log_buf->ptr = -1;
    no_rec = rvm_true;
    return retval;
    }
static rvm_return_t locate_prev_sub_rec()
    {
    rec_hdr_t       *rec_hdr;           /* generic record header */
    rvm_offset_t    offset;             /* offset temp. */
    long            incr;
    rvm_return_t    retval = RVM_SUCCESS; /* return code */

    /* get to known place */
    if (log_buf->ptr == -1)
        if (!reset_buffer(REVERSE))
            goto no_record;

    /* test if at start of log */
    offset = RVM_ADD_LENGTH_TO_OFFSET(log_buf->offset,log_buf->ptr);
    if (RVM_OFFSET_EQL(offset,log->status.log_start))
        {                               /* yes, find wrap marker */
        if ((retval=scan_wrap_reverse(log,NO_SYNCH)) != RVM_SUCCESS)
            goto no_record;
        return RVM_SUCCESS;
        }

    /* move to previous sub record */
    rec_hdr = (rec_hdr_t *)(&log_buf->buf[log_buf->ptr]);
    switch (rec_hdr->struct_id)
        {
      case nv_range_id:
        incr = ((nv_range_t *)rec_hdr)->sub_rec_len;
        break;
      case rec_end_id:
        incr = ((rec_end_t *)rec_hdr)->sub_rec_len;
        break;
      case trans_hdr_id: case log_seg_id: case log_wrap_id:
        incr = sizeof(rec_end_t);
        break;
      default:
        goto no_record;
        }

    /* set log buffer ptr to next sub record */
    if ((retval=load_sub_rec(incr,REVERSE)) == RVM_SUCCESS)
        return RVM_SUCCESS;

no_record:
    log_buf->ptr = -1;
    no_rec = rvm_true;
    return retval;
    }
/* locate modifications of vm address */
static rvm_bool_t locate_mods(vmaddr,length,nv_found,out_stream,err_stream)
    char            *vmaddr;            /* address to search for */
    rvm_length_t    length;             /* length of search range */
    nv_range_t      **nv_found;         /* range with located vmaddr [out] */
    FILE            *out_stream;
    FILE            *err_stream;
    {
    rec_hdr_t       *rec_hdr;           /* generic record header */
    nv_range_t      *nv;                /* nv range ptr */
    rvm_offset_t    offset;             /* offset arithmetic temp */
    char            *vm_end;            /* end of target range */
    char            *nv_end;            /* end of nv range */
    rvm_return_t    retval;

    /* initialize, load recovery buffer if necessary */
    *nv_found = NULL;
    vm_end = RVM_ADD_LENGTH_TO_ADDR(vmaddr,length);
    if ((log_buf->ptr < 0) || RVM_OFFSET_EQL_ZERO(log_buf->offset))
        if ((retval=init_buffer(log,&status->log_tail,REVERSE,
                                NO_SYNCH)) != RVM_SUCCESS)
            {
            pr_rvm_error(err_stream,retval,"initializing recovery buffer");
            return rvm_false;
            }

    offset = RVM_ADD_LENGTH_TO_OFFSET(log_buf->offset,log_buf->ptr);
    if (RVM_OFFSET_EQL(offset,status->log_tail))
        {
        if ((retval=scan_reverse(log,NO_SYNCH)) != RVM_SUCCESS)
            {
            pr_rvm_error(err_stream,retval,"scaning records");
            return rvm_false;
            }
        if (log_buf->ptr < 0)
            return no_rec=rvm_true;     /* no more records */
        }
    /* continue backwards through records, range by range */
    DO_FOREVER
        {
        if (chk_sigint(out_stream)) return rvm_true;

        /* get next sub record */
        if ((retval=locate_prev_sub_rec()) != RVM_SUCCESS)
            {
            pr_rvm_error(err_stream,retval,"scaning records");
            return rvm_false;
            }
        if (log_buf->ptr < 0)
            return no_rec=rvm_true;     /* no more records */

        /* see if have legit header */
        rec_hdr = (rec_hdr_t *)(&log_buf->buf[log_buf->ptr]);
        switch (rec_hdr->struct_id)
            {
          case nv_range_id: 
            /* check ranges for mods to target vmaddr range */
            nv = (nv_range_t *)rec_hdr;
            nv_end = RVM_ADD_LENGTH_TO_ADDR(nv->vmaddr,nv->length);
            if (((vmaddr >= nv->vmaddr) && (vmaddr < nv_end))
                || (((vm_end >= nv->vmaddr) && (vm_end < nv_end))))
                {
                *nv_found = nv;
                return rvm_true;
                }
            break;
          case trans_hdr_id: case rec_end_id:
          case log_seg_id: case log_wrap_id:
            set_log_position();
            break;
          default:
            offset = RVM_ADD_LENGTH_TO_OFFSET(log_buf->offset,
                                              log_buf->ptr);
            fprintf(err_stream,"\n? Unknown header type at log offset: ");
            prc_offset(err_stream,&offset,10,NULL);
            putc('\n',err_stream);
            return rvm_false;
            }
        }
    }
/* show earliest record */
static rvm_bool_t show_earliest(out_stream,err_stream)
    FILE            *out_stream;        /* output stream */
    FILE            *err_stream;        /* error output stream */
    {

    if (!locate_earliest(out_stream,err_stream))
        return rvm_false;
    if (chk_sigint(NULL)) goto exit;

    /* be sure something found */
    if (no_rec)
        {
        fprintf(out_stream,"  -- no more records\n");
        goto exit;
        }

    /* print what found */
    putc('\n',out_stream);
    print_cur_rec(out_stream,err_stream);

  exit:
    stop_sticky = rvm_true;
    return rvm_true;
    }
/* show header record support */
static rvm_bool_t show_head(out_stream,err_stream)
    FILE            *out_stream;        /* output stream */
    FILE            *err_stream;        /* error output stream */
    {
    rec_hdr_t       *rec_hdr;           /* generic record header */
    rvm_return_t    retval;

    /* initialize buffer at head of log */
    no_rec = rvm_true;
    if (!status->log_empty)
        {
        cur_direction = FORWARD;
        if ((retval=init_buffer(log,&status->log_head,
                                FORWARD,NO_SYNCH))
            != RVM_SUCCESS)
            {
            pr_rvm_error(err_stream,retval,"reading log head");
            goto no_record;
            }

        /* validate record */
        reset_hdr_chks(log);
        rec_hdr = (rec_hdr_t *)(&log_buf->buf[log_buf->ptr]);
        if ((validate_hdr(log,rec_hdr,NULL,FORWARD))
            && (!status->log_empty))
            {
            /* good record, print it */
            putc('\n',out_stream);
            print_cur_rec(out_stream,err_stream);
            return rvm_true;
            }
        else
            fprintf(out_stream,"  Head record damaged\n");
        }
    else
        fprintf(out_stream,"  Log is empty\n");

no_record:
    log_buf->ptr = -1;                  /* no valid record */
    no_rec = rvm_true;
    return rvm_false;
    }
/* show tail record support */
static rvm_bool_t show_tail(out_stream,err_stream)
    FILE            *out_stream;        /* output stream */
    FILE            *err_stream;        /* error output stream */
    {
    rvm_return_t    retval;

    /* initialize buffer at tail of log */
    no_rec = rvm_true;
    cur_direction = REVERSE;
    if ((retval=init_buffer(log,&status->log_tail,REVERSE,NO_SYNCH))
        != RVM_SUCCESS)
        {
        pr_rvm_error(err_stream,retval,"reading log tail");
        return rvm_false;
        }

    /* get last record */
    reset_hdr_chks(log);
    if ((retval=scan_reverse(log,NO_SYNCH))
        != RVM_SUCCESS)
        {
        pr_rvm_error(err_stream,retval,"scanning log file");
        return rvm_false;
        }

    /* see if there is a record */
    if (log_buf->ptr != -1)
        {
        /* yes, get head of record into buffer */
        if (load_rec_hdr(err_stream) != rvm_true)
            return rvm_false;
        putc('\n',out_stream);
        print_cur_rec(out_stream,err_stream);
        }
    else
        if (status->log_empty)
            fprintf(out_stream,"  Log is empty\n");
        else
            fprintf(out_stream,"  Tail record damaged\n");

    stop_sticky = rvm_true;
    return rvm_true;
    }
/* show by record number support */
static rvm_bool_t show_by_num(out_stream,err_stream,key)
    FILE            *out_stream;        /* output stream */
    FILE            *err_stream;        /* error output stream */
    key_id_t        key;                /* NUM/NEXT/PREV */
    {
    rec_hdr_t       *rec_hdr;           /* generic record header */
    long            rec_num = 0;        /* target record number */
    long            num;                /* number to print */
    rvm_return_t    retval=RVM_SUCCESS;

    /* check for numeric parameter(s) */
    num = str2ul(cmd_cur,&cmd_cur,NULL);
    if (key == NUM_KEY || key == REC_KEY) /* is there another ? */
        {
        rec_num = num;
        num = str2ul(cmd_cur,&cmd_cur,NULL); /* optional count */
        }
    if ((rec_num == 0) && (key == NUM_KEY || key == REC_KEY))
        {
        fprintf(err_stream,"?  record number required\n");
        return rvm_false;
        }
    if (num == 0) num = 1;              /* always try to print 1 record */

    /* see if must search for 1st record to print */
    while (num > 0)
        {
        switch (key)
            {
          case NUM_KEY:                 /* locate record by number */
	  case REC_KEY:
            if ((retval=locate_rec(rec_num,NULL)) != RVM_SUCCESS)
                break;
            stop_sticky = rvm_true;
            if (chk_sigint(NULL)) break;
            key = NEXT_KEY;
            if (log_buf->ptr == -1)
                fprintf(out_stream,"  -- no more records\n");
            break;

          case NEXT_KEY:                /* scan next record */
            if ((!RVM_OFFSET_EQL(cur_offset,status->log_head))
                && no_rec)
                return show_head(out_stream,err_stream);

            if ((retval=scan_forward(log,NO_SYNCH)) != RVM_SUCCESS)
                break;
            if (log_buf->ptr == -1)
                fprintf(out_stream,"  -- no next record\n");
            break;
          case PREV_KEY:                /* scan previous record */
            if ((!RVM_OFFSET_EQL(cur_offset,status->log_tail))
                && no_rec)
                return show_tail(out_stream,err_stream);

            if ((retval=scan_reverse(log,NO_SYNCH)) != RVM_SUCCESS)
                break;
            if (log_buf->ptr == -1)
                fprintf(out_stream,"  -- no previous record\n");
            break;

          default:      assert(rvm_false);  /* internal error */
            }

        /* report error if necessary */
        if (retval != RVM_SUCCESS)
            {
            pr_rvm_error(err_stream,retval,"scanning log file");
            no_rec = rvm_true;
            return rvm_false;
            }

        if (log_buf->ptr == -1)
            {
            stop_sticky = rvm_true;
            no_rec = rvm_true;
            return rvm_true;
            }
        if (chk_sigint(out_stream)) return rvm_true;

        /* get record header in buffer and print */
        rec_hdr = (rec_hdr_t *)&log_buf->buf[log_buf->ptr];
        if (rec_hdr->struct_id == rec_end_id)
            if (load_rec_hdr(err_stream) != rvm_true)
                return rvm_false;
        putc('\n',out_stream);
        print_cur_rec(out_stream,err_stream);
        num--;
        }
    return rvm_true;
    }
/* show by record timestamp support */
static rvm_bool_t show_by_time(out_stream,err_stream,key)
    FILE            *out_stream;        /* output stream */
    FILE            *err_stream;        /* error output stream */
    long            key;                /* ALL/REMAINING */
    {

    return rvm_true;
    }
/* show ALL support */
static rvm_bool_t show_all(out_stream,err_stream,key)
    FILE            *out_stream;        /* output stream */
    FILE            *err_stream;        /* error output stream */
    key_id_t        key;                /* ALL/REMAINING */
    {
    rvm_return_t    retval;

    /* initialize buffer at head of log */
    if (key == ALL_KEY)
        if (!show_head(out_stream,err_stream)) return rvm_false;
    if (log_buf->ptr == -1)
        return rvm_true;                    /* no more records */

    /* print the records */
    putc('\n',out_stream);
    DO_FOREVER
        {
        if ((retval=scan_forward(log)) != RVM_SUCCESS)
            {
            pr_rvm_error(err_stream,retval,"scanning log file");
            no_rec = rvm_true;
            return rvm_false;
            }
        if (log_buf->ptr == -1)
            {
            no_rec = rvm_true;
            break;
            }
        print_cur_rec(out_stream,err_stream);
        if (chk_sigint(out_stream)) break;
        }

    stop_sticky = rvm_true;
    return rvm_true;
    }
/* show sub record support */
static rvm_bool_t show_sub_rec(out_stream,err_stream,key)
    FILE            *out_stream;        /* output stream */
    FILE            *err_stream;        /* error output stream */
    key_id_t        key;
    {
    long            num;                /* number to print */
    rvm_return_t    retval = RVM_SUCCESS;

    /* get number of sub records to print */
    num = str2ul(cmd_cur,&cmd_cur,NULL);
    if (num == 0) num = 1;              /* always try to print 1 record */

    /* be sure positioned at known place in log */
    if (log_buf->ptr != -1)
        if (!reset_buffer(cur_direction))
            return rvm_false;

    /* print sub records */
    while (num > 0)
        {
            switch (key)
                {
              case SUB_KEY:             /* print current sub record */
                    {
                    key = NEXT_SUB_KEY;
                    break;
                    }
              case NEXT_SUB_KEY:        /* scan next sub record */
                    {
                    retval = locate_next_sub_rec();
                    break;
                    }
              case PREV_SUB_KEY:        /* scan previous sub record */
                    {
                    retval = locate_prev_sub_rec();
                    break;
                    }
              default:  assert(rvm_false);  /* internal error */
                }

        /* report error if necessary */
        if (retval != RVM_SUCCESS)
            {
            pr_rvm_error(err_stream,retval,"scanning log file");
            no_rec = rvm_true;
            return rvm_false;
            }
        /* print the sub record if valid */
        if (log_buf->ptr != -1)
            {
            print_cur_sub_rec(out_stream,err_stream);
            num--;
            }
        else
            {
            fprintf(out_stream,"  -- no more records\n");
            no_rec = rvm_true;
            stop_sticky = rvm_true;
            return rvm_true;
            }
        }
    return rvm_true;
    }
/* buffer manager for range data printing via log_buf.aux_buf */
static char *chk_aux_buf(offset,length,err_stream)
    rvm_offset_t    *offset;            /* initial offset in file */
    rvm_length_t    length;             /* printed line width */
    FILE            *err_stream;        /* error output stream */
    {
    long            ptr;                /* ptr to aux_buf */
    long            len;                /* length of data available */
    rvm_return_t    retval;

    DO_FOREVER
        {
        /* be sure new values are in aux buffer */
        if ((retval=load_aux_buf(log,offset,length,
                                 &ptr,&len,NO_SYNCH,rvm_true))
            != RVM_SUCCESS)
            {
            pr_rvm_error(err_stream,retval,"reading log");
            return NULL;
            }

        /* is there enough, or will we have to reload? */
        if (len >= length)
            return &log_buf->aux_buf[ptr]; /* enough */

        /* force aux_buf reload */
        clear_aux_buf(log);
        }
    }
/* match modifications in range with command line values */
static rvm_bool_t match_values(nv,offset,vmaddr,mod_cur,err_sw,err_stream)
    nv_range_t      *nv;                /* range header located */
    rvm_offset_t    *offset;            /* offset of data area in log */
    rvm_length_t    vmaddr;             /* modification address */
    char            *mod_cur;           /* cursor position for modifications */
    rvm_bool_t      *err_sw;            /* error return switch */
    FILE            *err_stream;        /* error output stream */
    {
    rvm_offset_t    temp;               /* temporary buffer for all integers */
    rvm_offset_t    cmp_offset;         /* comparison offset in log */
    rvm_length_t    len;                /* data length temp */
    char            *scan_ptr;          /* ptr to scanned data */
    char            *data_ptr;          /* ptr to data from log in aux_buf */
    rvm_length_t    lword;              /* scanning temps */
    double          dbl_float;
    float           sngl_float;
    rvm_bool_t      scanned_str;
    rvm_bool_t      scanned_float;

    /* initialize offsets, ptrs */
    *err_sw = rvm_false;
    cmd_cur = mod_cur;
    cmp_offset = *offset;
    if (!num_all)               /* adjust offset for vmaddr */
        cmp_offset = RVM_ADD_LENGTH_TO_OFFSET(cmp_offset,(rvm_length_t)
                        RVM_SUB_LENGTH_FROM_ADDR(vmaddr,nv->vmaddr));

    /* scan the values and compare with log data */
    while (!scan_stop(*cmd_cur))
        {
        if (!scan_poke_val(&temp,&scanned_str,&scanned_float,
                           &dbl_float,stderr))
            return *err_sw=rvm_true;        /* shouldn't ever happen */

        if (scanned_str)                /* string values */
            {
            len = strlen(temp_str);
            scan_ptr = temp_str;
            }
        else
            if (scanned_float)          /* float values */
                {
                if (num_format & (long)float_sw)
                    {
                    len = sizeof(float);
                    scan_ptr = (char *)&sngl_float;
                    sngl_float = dbl_float;
                    }
                else
                    {
                    len = sizeof(double);
                    scan_ptr = (char *)&dbl_float;
                    }
                }
            else                        /* integer values */
                if ((len=num_format_size(num_format))
                    <= sizeof(rvm_length_t))
                    {
                    lword = RVM_OFFSET_TO_LENGTH(temp);
                    scan_ptr = (char *)&lword;
                    }
                else scan_ptr = (char *)&temp;

        /* make data addressable and do comparison */
        if((data_ptr=chk_aux_buf(&cmp_offset,len,err_stream))== NULL)
            return *err_sw=rvm_true;
        if (memcmp(data_ptr,scan_ptr,len))
            return rvm_false;
        cmp_offset = RVM_ADD_LENGTH_TO_OFFSET(cmp_offset,len);
        }

    return rvm_true;
    }
/* show modifications support */
static rvm_bool_t show_mods(out_stream,err_stream)
    FILE            *out_stream;        /* output stream */
    FILE            *err_stream;        /* error output stream */
    {
    nv_range_t      *nv;                /* range header located */
    rvm_offset_t    temp;               /* temporary buffer for all integers */
    rvm_offset_t    offset;             /* log file offset */
    rvm_offset_t    limit;              /* end of data in log file */
    char            *vmaddr;            /* modification address */
    char            *mod_cur = NULL;    /* cursor position for modifications */
    int             base;               /* radix of vmaddr for printing */
    double          dbl_float;
    rvm_bool_t      err_sw;             /* error return indicator */

    /* get address */
    skip_white(&cmd_cur);
    if (scan_stop(*cmd_cur))
        {
        fprintf(stderr,"\n? No vm address given\n");
        return rvm_false;
        }
    vmaddr = (char *)RVM_OFFSET_TO_LENGTH(str2off(cmd_cur,&cmd_cur,&base));

    /* scan format specifier */
    skip_white(&cmd_cur);
    if ((*cmd_cur == '/'))
        {
        if (!scan_num_format())
            {
            bad_num_format(stderr);
            return rvm_false;
            }
        }
    else
        {
        if (num_format == 0)
            init_num_format();
        else num_count = 1;
        }

    /* pre-scan modification values */
    skip_white(&cmd_cur);
    if ((*cmd_cur == '='))
        {
        incr_cur(1);
        skip_white(&cmd_cur);
        mod_cur = cmd_cur;
        while (!scan_stop(*cmd_cur))
            {
            if (!scan_poke_val(&temp,&err_sw,&err_sw,
                               &dbl_float,stderr))
                return rvm_false;
            }
        if (mod_cur == cmd_cur)
            {
            fprintf(err_stream,"\n? No values specified\n");
            return rvm_false;
            }
        }

    /* if not a repeated line, force buffer to tail */
    if (!is_sticky_line)
        log_buf->ptr = -1;

    /* search for modifications */
    DO_FOREVER
        {
        if (!locate_mods(vmaddr,num_count,&nv,out_stream,err_stream))
            return rvm_false;           /* error while scanning */
        if (chk_sigint(NULL)) goto exit;
        if (nv == NULL)
            {
            fprintf(out_stream,"  -- no more records\n");
            goto exit;
            }

        /* set up offset of data in log */
        offset = RVM_ADD_LENGTH_TO_OFFSET(log_buf->offset,
                     sizeof(nv_range_t)+log_buf->ptr
                     +BYTE_SKEW(nv->vmaddr));
        if (!num_all)
            offset = RVM_ADD_LENGTH_TO_OFFSET(offset,(rvm_length_t)
                      RVM_SUB_LENGTH_FROM_ADDR(vmaddr,nv->vmaddr));

        /* match data with re-scan of specified values */
        if (mod_cur != NULL)
            {
            if (!match_values(nv,&offset,(rvm_length_t)vmaddr,mod_cur,
                              &err_sw,err_stream))
                goto next_sub_rec;
            if (err_sw) return rvm_false;

            /* print assignments located */
            fprintf(out_stream,"\nRecord number: %ld",nv->rec_hdr.rec_num);
            fprintf(out_stream," assigns specified values\n\n");
            }
        else
            {
            /* print record number and range */
            fprintf(out_stream,"\nRecord number: %ld ",nv->rec_hdr.rec_num);
            fprintf(out_stream,"modifies specified range:\n\n");
            }

            /* print located modifications */
            print_range(out_stream,nv);
            if (num_all)
                temp = RVM_LENGTH_TO_OFFSET(nv->vmaddr);
            else
                temp = RVM_LENGTH_TO_OFFSET(vmaddr);
            limit = RVM_ADD_LENGTH_TO_OFFSET(offset,nv->length);
            putc('\n',out_stream);
            return pr_data_range(out_stream,err_stream,2,LINE_WIDTH,
                                 &offset,&temp,base,num_count,
                                 chk_aux_buf,&limit,DATA_END_STR);
next_sub_rec:;
        }
exit:
    stop_sticky = rvm_true;
    return rvm_true;
    }
/* show monitored ranges support */
static rvm_bool_t show_monitor(out_stream,err_stream)
    FILE            *out_stream;        /* output stream */
    FILE            *err_stream;        /* error output stream */
    {
    int             i;
    unsigned long   format;

    /* see if range list if not empty */
    if (rvm_chk_len != 0)
        {
        /* no, print ranges */
        fprintf(out_stream,"\nRange    Address/format     Length\n");
        for (i=0; i<rvm_chk_len; i++)
            {
            if (chk_sigint(out_stream)) goto exit;
            fprintf(out_stream," %3d    ",i+1);
            pr_ulong(out_stream,(rvm_length_t)rvm_chk_vec[i].vmaddr,
                     rvm_chk_vec[i].radix,NULL);
            format = rvm_chk_vec[i].format;
            pr_format(out_stream,format,
                      rvm_chk_vec[i].length/num_format_size(format),
                      rvm_false);
            putc(' ',out_stream);
            pr_ulong(out_stream,rvm_chk_vec[i].length,10,NULL);
            putc('\n',out_stream);
            }
        }
    else
        fprintf(out_stream,"\n  Monitor range list is empty\n");

    putc('\n',out_stream);
  exit:
    stop_sticky = rvm_true;
    return rvm_true;
    }
/* show segment dictionary support */
static rvm_bool_t show_seg_dict(out_stream,err_stream)
    FILE            *out_stream;        /* output stream */
    FILE            *err_stream;        /* error output stream */
    {
    rvm_length_t    entry_num;

    /* see if dictionary empty */
    if (log->seg_dict_len == 0)
        {
        fprintf(out_stream,"\n  Segment dictionary empty\n");
        return rvm_true;
        }

    /* see if particular entry wanted */
    skip_white(&cmd_cur);
    if (!scan_stop(*cmd_cur))
        {
        if (isdigit(*cmd_cur))
            {
            entry_num = str2ul(cmd_cur,&cmd_cur,NULL);
            if (entry_num > log->seg_dict_len)
                {
                fprintf(err_stream,
                        "\n? Segment dictionary length is only %ld\n",
                        log->seg_dict_len);
                return rvm_false;
                }
            print_seg_dict(out_stream,err_stream,entry_num);
            return rvm_true;
            }
        else
            {
            fprintf(err_stream,"\n? Unsigned integer expected\n");
            return rvm_false;
            }
        }

    /* no parameter, print all entries */
    for (entry_num = 1; entry_num <= log->seg_dict_len; entry_num++)
        {
        if (chk_sigint(out_stream)) break;
        putc('\n',out_stream);
        print_seg_dict(out_stream,err_stream,entry_num);
        }

    return rvm_true;
    }
/* log status display */
static rvm_bool_t show_log_status(out_stream,err_stream)
    FILE            *out_stream;        /* output stream */
    FILE            *err_stream;        /* error stream */
    {
    rvm_offset_t    size_temp;          /* size calculation temp */
    rvm_offset_t    size_temp2;         /* size calculation temp */

    /* print log device basics */
    if (no_log()) return rvm_false;
    print_log_hdr(out_stream,err_stream);

    /* print head and tail offsets */
    fprintf(out_stream,"  log head offset:       ");
    prc_offset(out_stream,&status->log_head,10,NULL);
    putc('\n',out_stream);
    fprintf(out_stream,"\n");
    if (!RVM_OFFSET_EQL_ZERO(status->prev_log_head))
        {
        fprintf(out_stream,"  previous head offset:  ");
        prc_offset(out_stream,&status->prev_log_head,10,NULL);
        fprintf(out_stream,"\n");
        }
    fprintf(out_stream,"  log tail offset:       ");
    prc_offset(out_stream,&status->log_tail,10,NULL);
    fprintf(out_stream,"\n");
    if (!RVM_OFFSET_EQL_ZERO(save_prev_log_tail))
        {
        fprintf(out_stream,"  previous tail offset:  ");
        prc_offset(out_stream,&save_prev_log_tail,10,NULL);
        fprintf(out_stream,"\n");
        }
    fprintf(out_stream,"  log empty:             ");
    pr_bool(out_stream,status->log_empty,NULL);
    fprintf(out_stream,"\n\n");
    /* compute & print what used and available */
    if (!RVM_OFFSET_EQL_ZERO(status->prev_log_head))
        size_temp2 = status->prev_log_head;
    else
        size_temp2 = status->log_head;
    fprintf(out_stream,"  space used by records: ");
    if (RVM_OFFSET_GEQ(status->log_tail,size_temp2))
        size_temp = RVM_SUB_OFFSETS(status->log_tail,size_temp2);
    else                                /* log wrap around */
        {
        size_temp = RVM_SUB_OFFSETS(log_dev->num_bytes,
                                    size_temp2);
        size_temp = RVM_ADD_OFFSETS(size_temp,status->log_tail);
        size_temp = RVM_SUB_OFFSETS(size_temp,status->log_start);
        }
    prc_offset(out_stream,&size_temp,10,NULL);
    putc('\n',out_stream);
    fprintf(out_stream,"  space available:       ");
    size_temp = RVM_SUB_OFFSETS(status->log_size,size_temp);
    prc_offset(out_stream,&size_temp,10,NULL);
    putc('\n',out_stream);

    /* print other sizes */
    fprintf(out_stream,"  status area size:      %10ld\n",
            LOG_DEV_STATUS_SIZE);
    if (log_dev->raw_io)
        fprintf(out_stream,"  status area offset:    %10d\n",
                RAW_STATUS_OFFSET);
    fprintf(out_stream,"  total log size:        ");
    prc_offset(out_stream,&log_dev->num_bytes,10,NULL);
    fprintf(out_stream,"\n\n");

    /* print content status if log not empty */
    fprintf(out_stream,"  first record number:   %10ld\n",
            status->first_rec_num);
    fprintf(out_stream,"  last record number:    %10ld\n",
            status->next_rec_num-1);
    if (!status->log_empty)
        {
        fprintf(out_stream,"  first timestamp:       ");
        if (!TIME_EQL_ZERO(status->first_write))
            fprintf(out_stream,"null");
        else
            pr_timeval(out_stream,&status->first_write,rvm_true,NULL);
        fprintf(out_stream,"\n");
        fprintf(out_stream,"  last  timestamp:       ");
        if (!TIME_EQL_ZERO(status->last_write))
            fprintf(out_stream,"null");
        else
            pr_timeval(out_stream,&status->last_write,rvm_true,NULL);
        putc('\n',out_stream);
        fprintf(out_stream,"  first trans. uname:    ");
        if (!TIME_EQL_ZERO(status->first_uname))
            fprintf(out_stream,"null");
        else
            prw_timeval(out_stream, &status->first_uname);
        putc('\n',out_stream);
        fprintf(out_stream,"  last  trans. uname:    ");
        if (!TIME_EQL_ZERO(status->last_uname))
            fprintf(out_stream,"null");
        else
            prw_timeval(out_stream, &status->last_uname);
        putc('\n',out_stream);
        }

    return rvm_true;
    }
/* show statistics */
static rvm_bool_t show_statistics(out_stream,err_stream)
    FILE            *out_stream;        /* output stream */
    FILE            *err_stream;        /* error stream */
    {
    rvm_statistics_t *stats;            /* statistics record */
    rvm_return_t    retval=RVM_SUCCESS; /* rvm return value */

    /* check for redirection of output & print proper hreader */
    print_log_hdr(out_stream,err_stream); /* print log basics */

    /* get the statistics */
    if ((stats=rvm_malloc_statistics()) == NULL)
        {
        pr_rvm_error(err_stream,retval,"getting statistics record");
        return rvm_false;
        }
    if ((retval=RVM_STATISTICS(stats)) != RVM_SUCCESS)
        {
        if (retval == RVM_ESTAT_VERSION_SKEW)
            {
            fprintf(err_stream,"? rvmutl built with wrong librvm\n");
            fprintf(err_stream,"  Compile statistics version is: %s\n",
                    RVM_STATISTICS_VERSION);
/*            fprintf(err_stream,"  Library statistics version is: %s\n",
                    &rvm_statistics_version);
*/
            }
        else
            pr_rvm_error(err_stream,retval,"getting statistics");
        return rvm_false;
        }

    /* print current statistics */
    retval=rvm_print_statistics(stats,out_stream);
    fprintf(out_stream,"\n");
    rvm_free_statistics(stats);
    if (retval != RVM_SUCCESS)
        {
        pr_rvm_error(err_stream,retval,"while printing statistics");
        return rvm_false;
        }

    return rvm_true;
    }
/* show record key words */
#define MAX_SHOW_KEYS   32              /* maximum number of key words */

static str_name_entry_t show_key_vec[MAX_SHOW_KEYS] =
                    {
                    {"all",(char *)ALL_KEY},
                    {"all_records",(char *)ALL_KEY},
                    {"earliest",(char *)EARLY_KEY},
                    {"head",(char *)HEAD_KEY},
                    {"mods",(char *)MODS_KEY},
                    {"modifications",(char *)MODS_KEY},
                    {"monitor",(char *)MONITOR_KEY},
                    {"next",(char *)NEXT_KEY},
                    {"next_sub_rec",(char *)NEXT_SUB_KEY},
                    {"ns",(char *)NEXT_KEY},
                    {"prev",(char *)PREV_KEY},
                    {"previous",(char *)PREV_KEY},
                    {"ps",(char *)PREV_SUB_KEY},
                    {"prev_sub_rec",(char *)PREV_SUB_KEY},
                    {"rec_number",(char *)REC_KEY},
                    {"record_number",(char *)REC_KEY},
                    {"remaining",(char *)REMAINING_KEY},
                    {"seg_dict",(char *)SEG_DICT_KEY},
                    {"segment_dictionary",(char *)SEG_DICT_KEY},
                    {"statistics",(char *)STATISTICS_KEY},
                    {"status",(char *)LOG_STATUS_KEY},
                    {"log_status",(char *)LOG_STATUS_KEY},
                    {"sr",(char *)SUB_KEY},
                    {"sub_rec",(char *)SUB_KEY},
                    {"tail",(char *)TAIL_KEY},
                    {"timestamp",(char *)TIME_KEY},
                    {"uname",(char *)UNAME_KEY},
                    {"",NULL}           /* end mark, do not delete */
                    };
    
/* show log record command support*/
static rvm_bool_t do_show()
    {
    long            key_index;          /* index of key word in vector */
    key_id_t        key;                /* actual key */
    FILE            *out_stream;        /* output stream */
    FILE            *err_stream;        /* error output stream */
    rvm_bool_t      no_err = rvm_true;

    /* be sure log is open & not empty */
    if (no_log()) return rvm_false;

    /* see if redirection wanted */
    get_redirect(&out_stream,&err_stream);
    if (out_stream != stdout)
        fprintf(out_stream,"\f");

    /* process key words & print */
    DO_FOREVER
        {
        skip_white(&cmd_cur);
        if (scan_stop(*cmd_cur)) goto exit;
        if (isdigit(*cmd_cur))
            key = NUM_KEY;
        else
            {
            key_index = scan_str_name(NULL,show_key_vec,KEY_WORD_STR);
            if (key_index == (long)UNKNOWN) continue;
            key = (key_id_t)show_key_vec[key_index].target;
            }

        /* interpret request */
        switch (key)
            {
          case ALL_KEY:                 /* print all records */
            if (!show_all(out_stream,err_stream,ALL_KEY))
                goto err_exit;
            break;
          case HEAD_KEY:                /* print head (1st) record */
            if (!show_head(out_stream,err_stream)) goto err_exit;
            break;
          case NEXT_KEY:                /* print next record(s) */
          case PREV_KEY:                /* print previous record(s) */
	  case REC_KEY:
          case NUM_KEY:                 /* print record selected by number */
            if (!show_by_num(out_stream,err_stream,key))
                goto err_exit;
            break;
          case SUB_KEY:                 /* print current sub record */
          case NEXT_SUB_KEY:            /* print next sub record */
          case PREV_SUB_KEY:            /* print previous sub record */
            if (!show_sub_rec(out_stream,err_stream,key))
                goto err_exit;
            break;
          case TAIL_KEY:                /* print tail (last) record */
            if (!show_tail(out_stream,err_stream)) goto err_exit;
            break;
          case MODS_KEY:                /* print modifications of vm */
            if (!show_mods(out_stream,err_stream)) goto err_exit;
            break;
          case TIME_KEY:                /* print rec selected by timestamp */
          case UNAME_KEY:               /* print rec selected by uname */
            if (!show_by_time(out_stream,err_stream,key))
                goto err_exit;
            break;
          case EARLY_KEY:
            if (!show_earliest(out_stream,err_stream)) goto err_exit;
            break;
          case REMAINING_KEY:
            if (!show_all(out_stream,err_stream,REMAINING_KEY))
                goto err_exit;
            break;
          case MONITOR_KEY:
            if (!show_monitor(out_stream,err_stream))
                goto err_exit;
            break;
          case SEG_DICT_KEY:
            if (!show_seg_dict(out_stream,err_stream))
                goto err_exit;
            break;
          case LOG_STATUS_KEY:
            if (!show_log_status(out_stream,err_stream))
                goto err_exit;
            break;
          case STATISTICS_KEY:
            if (!show_statistics(out_stream,err_stream))
                goto err_exit;
            break;
          default: assert(rvm_false);       /* trouble... */
            }
        }
  err_exit: no_err = rvm_false;
  exit:
    close_redirect(out_stream,err_stream);
    return no_err;
    }
/* next, prev command support */
static rvm_bool_t do_next()
    {
    /* be sure log is open & not empty, buffer initialized */
    if (no_log()) return rvm_false;
    if (!reset_buffer(FORWARD)) return rvm_false;

    /* print next record */
    return show_by_num(stdout,stderr,NEXT_KEY);
    }

static rvm_bool_t do_prev()
    {
    /* be sure log is open & not empty, buffer initialized */
    if (no_log()) return rvm_false;
    if (!reset_buffer(REVERSE)) return rvm_false;

    /* print previous record */
    return show_by_num(stdout,stderr,PREV_KEY);
    }
static rvm_bool_t do_head()
    {
    if (no_log()) return rvm_false;
    return show_head(stdout,stderr);
    }

static rvm_bool_t do_tail()
    {
    if (no_log()) return rvm_true;
    return show_tail(stdout,stderr);
    }

static rvm_bool_t do_earliest()
    {
    if (no_log()) return rvm_true;
    return show_earliest(stdout,stderr);
    }

static rvm_bool_t do_sub_rec()
    {
    if (no_log()) return rvm_true;
    return show_sub_rec(stdout,stderr,SUB_KEY);
    }

static rvm_bool_t do_next_sub_rec()
    {
    if (no_log()) return rvm_true;
    return show_sub_rec(stdout,stderr,NEXT_SUB_KEY);
    }

static rvm_bool_t do_prev_sub_rec()
    {
    if (no_log()) return rvm_true;
    return show_sub_rec(stdout,stderr,PREV_SUB_KEY);
    }

static rvm_bool_t do_show_statistics()
    {
    if (no_log()) return rvm_false;
    return show_statistics(stdout,stderr);
    }

static rvm_bool_t do_show_log_status()
    {
    if (no_log()) return rvm_false;
    return show_log_status(stdout,stderr);
    }
/* set command key words */
#define MAX_SET_KEYS   20               /* maximum number of key words */

/* these key words must not begin with 'x' or 'X' */
static str_name_entry_t set_key_vec[MAX_SHOW_KEYS] =
                    {
                    {"head",(char *)HEAD_KEY},
                    {"log_start",(char *)LOG_START_KEY},
                    {"prev",(char *)PREV_KEY},
                    {"previous",(char *)PREV_KEY},
                    {"prev_head",(char *)PREV_HEAD_KEY},
                    {"prev_tail",(char *)PREV_TAIL_KEY},
                    {"tail",(char *)TAIL_KEY},
                    {"segment",(char *)SEG_DICT_KEY},
                    {"seg_dict",(char *)SEG_DICT_KEY},
                    {"",NULL}           /* end mark, do not delete */
                    };

/* segment dictionary definition support */
static rvm_bool_t scan_seg_dict_index(length_ptr)
    rvm_length_t    *length_ptr;           /* seg_dict index [out] */
    {

    /* scan index value */
    skip_white(&cmd_cur);
    if (isdigit(*cmd_cur) || (*cmd_cur == 'x') || (*cmd_cur == 'X'))
        {
        *length_ptr = str2l(cmd_cur,&cmd_cur,NULL);
        if ((long)(*length_ptr) >= 0) return rvm_true;
        }

    fprintf(stderr,
            "\n? Positive integer segment code must be specified\n");
    return rvm_false;
    }

static rvm_bool_t scan_seg_dict_file(index)
    long            index;              /* seg_dict index */
    {
    char            file_name[MAXPATHLEN+1];
    FILE            *temp;
    long            len;
    rvm_return_t    retval;

    /* get file name */
    len = scan_str(file_name,MAXPATHLEN);
    if (len == 0)
        {
        fprintf(stderr,"\n? File name must be specified\n");
        return rvm_false;
        }

    /* make sure the file can be opened during recovery */
    if (rvm_no_update)
        temp = fopen(file_name,"r");
    else
        temp = fopen(file_name,"r+");
    if (temp == NULL)
        {
        pr_rvm_error(stderr,RVM_EIO,"opening segment file");
        return rvm_false;
        }
    fclose(temp);

    /* put file name in seg_dict slot specified by index */
    if ((retval=enter_seg_dict(log,index)))
        goto err_exit;
    if ((log->seg_dict_vec[index].dev.name=malloc(len)) == NULL)
        {
        retval = RVM_ENO_MEMORY;
        goto err_exit;
        }
    (void)BCOPY(file_name,log->seg_dict_vec[index].dev.name,len);
    return rvm_true;

err_exit:
    pr_rvm_error(stderr,retval,"defining segment dictionary");
    return rvm_false;
    }
/* scan field identifiers for set command */
static rvm_bool_t scan_set_field(offset_ptr,length_ptr,key_ptr)
    rvm_offset_t    **offset_ptr;       /* ptr to offset [out] */
    rvm_length_t    *length_ptr;        /* ptr to word [out] */
    rvm_length_t    *key_ptr;           /* index of key word [out] */
    {
    char            str_name_buf[STR_NAME_LEN+1]; /* name buffer */
    rvm_bool_t      prev = rvm_false;

    /* scan first key word */
    *key_ptr = scan_str_name(str_name_buf,set_key_vec,"key word");
    switch ((long)set_key_vec[*key_ptr].target)
        {
      case SEG_DICT_KEY:
        return scan_seg_dict_index(length_ptr);
      case PREV_KEY:
        prev = rvm_true;
        *key_ptr = scan_str_name(str_name_buf,set_key_vec,"key word");
        break;
        }

    /* dispatch on last key word scanned & set *xxx_ptr */
    switch ((long)set_key_vec[*key_ptr].target)
        {
      case PREV_HEAD_KEY:
        prev = rvm_true;
      case HEAD_KEY:
        if (prev) *offset_ptr = &status->prev_log_head;
        else *offset_ptr = &status->log_head;
        return rvm_true;
      case LOG_START_KEY:
        *offset_ptr = &status->log_start;
        return rvm_true;
      case PREV_TAIL_KEY:
        prev = rvm_true;
      case TAIL_KEY:
        if (prev) *offset_ptr = &status->prev_log_tail;
        else *offset_ptr = &status->log_tail;
        return rvm_true;
      default:
        bad_key_word(str_name_buf,NOT_MEANINGFUL_STR,stderr);
       }

    return rvm_false;
    }
/* set command support */
static rvm_bool_t do_set()
    {
    rvm_offset_t    *src;
    rvm_offset_t    *dest;
    rvm_offset_t    offset;
    rvm_length_t    len;
    long            key_index;

    /* scan key word(s) for destination */
    if (!scan_set_field(&dest,&len,&key_index)) return rvm_false;

    /* look for '=' */
    skip_white(&cmd_cur);
    if (*cmd_cur != '=')
        {
        fprintf(stderr,"\n? Key word or index must be followed by '='\n");
        return rvm_false;
        }
    incr_cur(1);

    /* check for special source processing */
    switch ((long)set_key_vec[key_index].target)
        {
      case SEG_DICT_KEY:
        return scan_seg_dict_file(len);
        }
    
    /* see if source is a value or value-specifying key word & scan */
    skip_white(&cmd_cur);
    if (isdigit(*cmd_cur) || (*cmd_cur == 'x') || (*cmd_cur == 'X'))
        offset = str2off(cmd_cur,&cmd_cur,NULL);
    else
        {
        if (!scan_set_field(&src,&len,&key_index)) return rvm_false;
        if ((long)set_key_vec[key_index].target == (long)SEG_DICT_KEY)
            {
            fprintf(stderr,"\n? Segment dictionary not valid as rhs\n");
            return rvm_false;
            }
        offset = *src;
        }

    /* assign new value */
    *dest = offset;
    status->log_empty = (rvm_bool_t)RVM_OFFSET_EQL(status->log_head,
						   status->log_tail);
    return rvm_true;
    }
/* build_seg_dict command and support */
#define MAX_SEG_DICT_KEYS   10          /* maximum number of key words */

static str_name_entry_t seg_dict_key_vec[MAX_SEG_DICT_KEYS] =
                    {
                    {"all",(char *)ALL_KEY},
                    {"clear",(char *)CLEAR_KEY},
                    {"",NULL}           /* end mark, do not delete */
                    };

static rvm_bool_t do_build_seg_dict()
    {
    rvm_offset_t    offset;             /* offset temporary */
    rec_end_t       *rec_end;           /* standard record ender */
    rec_hdr_t       *rec_hdr;           /* standard record header */
    log_seg_t       *log_seg;           /* log segment definition descriptor */
    seg_dict_t      *seg_dict;          /* segment dictionary entry */
    char            *cur_msg;           /* log seg record currency msg */
    long            key;                /* key word index */
    rvm_bool_t      all_sw = rvm_false; /* print all log_seg's if true */
    rvm_bool_t      clear_sw = rvm_false; /* clear seg_dict if true */
    rvm_return_t    retval;

    /* see if we have a repeat line */
    skip_white(&cmd_cur);
    if (scan_stop(*cmd_cur) && (!is_sticky_line))
        is_sticky_line = rvm_true;      /* process empty line like sticky repeat */
    else
        /* scan key words */
        DO_FOREVER
            {
            skip_white(&cmd_cur);
            if (scan_stop(*cmd_cur)) break;
            key = scan_str_name(NULL,seg_dict_key_vec,KEY_WORD_STR);
            if (key == (long)UNKNOWN) return rvm_false;
            switch ((long)seg_dict_key_vec[key].target)
                {
              case ALL_KEY:     all_sw = rvm_true; break;
              case CLEAR_KEY:   clear_sw = rvm_true; break;
              default:          assert(rvm_false);
                }
            }

    /* release existing seg_dict and initialize if not repeated search */
    if (!is_sticky_line || no_rec || clear_sw)
        {
        log->seg_dict_len = 0;
        if (log->seg_dict_vec != NULL)
            {
            free(log->seg_dict_vec);
            log->seg_dict_vec = NULL;
            }

        /* init buffer at tail, reverse direction */
        cur_direction = REVERSE;
        cur_offset = status->log_tail;
        if ((retval=init_buffer(log,&cur_offset,cur_direction,NO_SYNCH))
            != RVM_SUCCESS)
            {
            pr_rvm_error(stderr,retval,INIT_BUF_STR);
            return rvm_false;
            }
        }
    /* scan reverse for log_seg records */
    DO_FOREVER
        {
        if (chk_sigint(stdout)) break;
        if ((retval=scan_reverse(log,NO_SYNCH)) != RVM_SUCCESS)
            goto log_err_exit;
        if (log_buf->ptr == -1) 
            {
            no_rec = rvm_true;
            break;  /* done */
            }

        /* see if found tail */
        offset = RVM_ADD_LENGTH_TO_OFFSET(log_buf->offset,
                                              log_buf->ptr);
        rec_end = (rec_end_t *)set_log_position();
        if (rec_end->rec_hdr.struct_id == rec_end_id)
            {
            offset = RVM_ADD_LENGTH_TO_OFFSET(offset,
                                              rec_end->rec_hdr.rec_length);
            if (RVM_OFFSET_EQL(offset,status->log_tail))
                {
                no_rec = rvm_true;
                break;                  /* yes, all records scanned */
                }
            }

        /* see what was found */
        switch (rec_end->rec_hdr.struct_id)
            {
          case log_wrap_id: continue;
          case rec_end_id: break;
          default: assert(rvm_false);
            }
        switch (rec_end->rec_type)
            {
          case trans_hdr_id: continue;
          case log_seg_id: break;
          default: assert(rvm_false);
            }
        /* have log segment dictionary defintion */
        retval = load_sub_rec(rec_end->rec_hdr.rec_length, REVERSE);
        if (retval != RVM_SUCCESS) goto log_err_exit;
        cur_msg = "(obsolete)";
        rec_hdr = set_log_position();
        log_seg = (log_seg_t *)((rvm_length_t)rec_hdr+sizeof(rec_hdr_t));
        seg_dict = &log->seg_dict_vec[SEG_DICT_INDEX(log_seg->seg_code)];
        if ((log->seg_dict_len < log_seg->seg_code)
            || (seg_dict->struct_id != seg_dict_id))
            cur_msg = "(definition)";
        if ((retval=def_seg_dict(log,rec_hdr)) != RVM_SUCCESS)
            {
            pr_rvm_error(stderr,retval,"defining segment dictionary entry");
            return rvm_false;
            }

        /* print entry scanned */
        print_rec(stdout,stderr,rec_hdr,cur_msg);
        if (!all_sw) break;
        }

    if (chk_sigint(NULL)) return rvm_true;
    if (no_rec)
        {
        printf("\n  Segment dictionary scan complete\n");
        stop_sticky = rvm_true;
        }
    return rvm_true;
        
log_err_exit:
    pr_rvm_error(stderr,retval,LOG_SCAN_STR);
    return rvm_false;
    }
/* file length scanner */
static rvm_length_t get_file_length(out_stream,prompt)
    FILE            *out_stream;
    char            *prompt;            /* prompt string */
    {
    rvm_length_t    log_len;            /* length for log file */

    /* get length of file */
    DO_FOREVER
        {
        log_len = 0;
        read_prompt_line(out_stream,prompt,rvm_false);

        /* evaluate and check length */
        log_len = str2ul(cmd_cur,&cmd_cur,NULL);

        /* get size code & multiply if specified */
        skip_white(&cmd_cur);
        DO_FOREVER
            {
            switch (*cmd_cur)
                {
              case 'k': case 'K':       /* size in k */
                {
                log_len = log_len*1024; break;
                }
              case 'm': case 'M':       /* size in megabytes */
                {
                log_len = log_len*1048576; break;
                }
              case 's': case 'S':       /* size in sectors */
                {
                log_len = log_len*SECTOR_SIZE; break;
                }
              case 'p': case 'P':       /* size in pages */
                {
                log_len = log_len*page_size; break;
                }
              case 'b': case 'B': break; /* bytes */
              case '\0': case '\n': break; /* no code, assume bytes */
              default:
                {
                fprintf(stderr,"? unknown size specifier: %c\n",*cmd_cur);
                read_prompt_line(out_stream,
                                 "Enter size code (b,k,m,s,p,<null>):",
                                 rvm_true);
                continue;
                }
                }
            break;
            }
        /* round up to integral sector size */
        log_len = CHOP_TO_SECTOR_SIZE(log_len);
        if (log_len > SECTOR_SIZE) break;
        fprintf(stderr,"? log length must be at least one sector\n");
        }

    return log_len;
    }

#ifdef UNUSED_FUNCTIONS
/* device length lookup */
static rvm_length_t get_dev_length(dev_name)
    char            *dev_name;           /* device name string */
    {
    rvm_length_t    log_len;            /* length of log device */

    log_len = 0;
    printf("? device lookup not yet implemented: %s\n",dev_name);
    exit(1);

    return log_len;
    }
#endif
/* init_log command support */
static rvm_bool_t do_init_log()
{
    log_dev_status_t *log_dev_status = (log_dev_status_t *)status_io;
    rvm_length_t    log_len;            /* length of log file/device */
    rvm_offset_t    offset;             /* offset temporary */
    long            *buf;               /* log init buffer */
    rvm_length_t    wrt_len;            /* length of log init writes */
    rvm_return_t    retval;
    long            i;

    /* get name of log file or device */
    read_prompt_line(stdout,LOG_NAME_PROMPT,rvm_false);
    (void)scan_str(dev_str,MAXPATHLEN);

    /* get length of log device or file */
    log_len = LOG_DEV_STATUS_SIZE + CHOP_TO_SECTOR_SIZE(
                get_file_length(stdout,"Enter length of log data area:"));

    /* build internal descriptors */
    if ((log = make_log(dev_str,&retval)) == NULL)
        {                               /* can't make descriptor */
        pr_rvm_error(stderr,retval,"while building log descriptor\n");
        exit(1);
        }
    log_dev = &log->dev;
    status = &log->status;
    log_buf = &log->log_buf;

    /* open the file or device and set characteristcs */
    if (open_dev(log_dev,O_WRONLY | O_CREAT | O_TRUNC,mode) != 0)
        {
        perror("? cannot open file/device");
        exit(1);
        }
    if (set_dev_char(log_dev,NULL) < 0)
        {
        perror("? cannot get file/device characteristics");
        exit(1);
        }

    /* get location of log status area and set total length of log */
    if (log_dev->raw_io)
        /* log is a partition, skip system label and boot sectors */
        log_len += RAW_STATUS_OFFSET;
    else
        log_len += FILE_STATUS_OFFSET;
    log_dev->num_bytes = RVM_MK_OFFSET(0,log_len);
    /* initialize log status area */
    if ((retval=init_log_status(log)) !=  RVM_SUCCESS)
        {
        perror("? could not initialize log status area");
        exit(1);
        }

    /* init buffer for clearing data area of log */
    log_buf->length = RECOVERY_BUF_LEN;
    if ((buf=(long *)page_alloc(log_buf->length)) == NULL)
        {
        fprintf(stderr,"?  Could not allocate space to init log\n");
        exit(1);
        }
    for (i=0;i<((log_buf->length)/sizeof(long));i++)
        buf[i] = -1;

    /* init log data area */
    offset = status->log_start;
    while (RVM_OFFSET_LSS(offset,log_dev->num_bytes))
        {
        wrt_len = RVM_OFFSET_TO_LENGTH(
                      RVM_SUB_OFFSETS(log_dev->num_bytes,offset));
        if (wrt_len > log_buf->length)
            wrt_len = log_buf->length;

        if (write_dev(log_dev,&offset,buf,wrt_len,NO_SYNCH) < 0)
            {
            perror("? could not initialize log data area");
                exit(1);
            }
        offset = RVM_ADD_LENGTH_TO_OFFSET(offset,wrt_len);
        }
    if (sync_dev(log_dev) < 0)
        {
        perror("? could not initialize log data area");
        exit(1);
        }
    page_free((char *)buf,log_buf->length);

    /* leave version string in local status i/o area for status printing */
    strcpy(log_dev_status->version, RVM_VERSION);

    /* enter log in log list */
    enter_log(log);
    return rvm_true;
}
/* log status area primatives */
static rvm_bool_t find_tail(out_stream,err_stream)
    FILE            *out_stream;
    FILE            *err_stream;
    {
    rvm_return_t    retval;

    CRITICAL(log->dev_lock,
        {
        log->status.trunc_state = ZERO;
        retval=locate_tail(log);
        log->status.trunc_state = ZERO;
        });
    if (retval != RVM_SUCCESS)
        {
        pr_rvm_error(stderr,retval,"locating log tail");
        return rvm_false;
        }
    return rvm_true;
    }
static rvm_bool_t do_find_tail()
    {

    if (no_log()) return rvm_false;

    if (!find_tail(stdout,stderr))
        return rvm_false;

    /* print tail displacement in log */
    fprintf(stdout,"  Tail at:");
    prc_offset(stdout,&status->log_tail,10,NULL);
    putchar('\n');
    status->prev_log_tail = status->log_tail; /* for scan_reverse */
    cur_offset = status->log_tail;

    return rvm_true;
    }

static rvm_bool_t do_show_all_spec()
{
    rec_hdr_t           *rec_hdr;
    rvm_return_t         retval = RVM_SUCCESS;

    if (no_log()) return rvm_false;

    /* initialize buffer at tail of log */
    no_rec = rvm_true;
    cur_direction = REVERSE;
    if ((retval=init_buffer(log,&status->log_tail,REVERSE,NO_SYNCH))
        != RVM_SUCCESS)
        {
        pr_rvm_error(stderr,retval,"reading log tail");
        return rvm_false;
        }

    /* get last record */
    reset_hdr_chks(log);
    if ((retval=scan_reverse(log,NO_SYNCH))
        != RVM_SUCCESS)
        {
        pr_rvm_error(stderr,retval,"scanning log file");
        return rvm_false;
        }

    /* While there are records... */
    while (log_buf->ptr != -1) {
	/* Okay, we now are at the tail, and can go backwards */
	
	if (load_rec_hdr(stderr) != rvm_true)
	    return rvm_false;
	rec_hdr = set_log_position();
	switch(rec_hdr->struct_id)
	{
	case trans_hdr_id:
	    break;
	default:
	    print_cur_rec(stdout,stderr);
	}            
	
	if ((retval=scan_reverse(log,NO_SYNCH)) != RVM_SUCCESS)
	    break;
	if (chk_sigint(stdout)) return rvm_true;
    }
    return rvm_true;
}

static rvm_bool_t do_find_hole()
{
    rvm_return_t retval;
    rec_hdr_t   *rec_hdr;

    if (no_log()) return rvm_false;

    /* initialize buffer at tail of log */
    no_rec = rvm_true;
    cur_direction = REVERSE;
    if ((retval=init_buffer(log,&status->log_tail,REVERSE,NO_SYNCH))
        != RVM_SUCCESS)
        {
        pr_rvm_error(stderr,retval,"reading log tail");
        return rvm_false;
        }

    /* get last record */
    reset_hdr_chks(log);
    if ((retval=scan_reverse(log,NO_SYNCH))
        != RVM_SUCCESS)
        {
        pr_rvm_error(stderr,retval,"scanning log file");
        return rvm_false;
        }

    /* While there are records... */
    while (log_buf->ptr != -1) {
	if (load_rec_hdr(stderr) != rvm_true)
	    return rvm_false;
	rec_hdr = set_log_position();

	/* Do something */
	/* XXX ! */
	if (chk_sigint(stdout)) return rvm_true;
    }
    return rvm_true;
}

static rvm_bool_t do_find_earliest()
    {

    if (no_log()) return rvm_false;

    if (!locate_earliest(stdout,stderr))
        return rvm_false;
    if (chk_sigint(NULL)) return rvm_true;

    if (!no_rec)
        {
        /* print record displacement in log */
        printf("  Earliest record at:");
        prc_offset(stdout,&cur_offset,10,NULL);
        putchar('\n');
        }
    else
        if (!reset_buffer(cur_direction))
            return rvm_false;

    return rvm_true;
    }
/* recovery monitoring support */
#define MAX_MONITOR_KEYS   20           /* maximum number of key words */

static str_name_entry_t monitor_key_vec[MAX_MONITOR_KEYS] =
                    {
                    {"clear",(char *)CLEAR_KEY},
                    {"no_update",(char *)NO_UPDATE_KEY},
                    {"update",(char *)UPDATE_KEY},
                    {"",NULL}           /* end mark, do not delete */
                    };

/* get monitor redirection files */
static void redirect_monitor()
    {
    if (monitor_out == NULL) monitor_out = stdout;
    if (monitor_err == NULL) monitor_err = stderr;
    get_redirect(&monitor_out,&monitor_err);
    }

/* terminate monitoring */
static void close_monitor()
    {
    close_redirect(monitor_out,monitor_err);
    monitor_out = NULL;
    monitor_err = NULL;
    }

/* terminate monitoring */
static void clear_monitor()
    {
    close_monitor();
    rvm_chk_len = 0;
    }

static char *chk_monitor(offset,length,err_stream)
    rvm_offset_t    *offset;            /* initial offset in file */
    rvm_length_t    length;             /* data length needed */
    FILE            *err_stream;        /* error output stream */
    {
    /* see if data is in vm */
    if (monitor_vm)
        return (char *)RVM_OFFSET_TO_LENGTH(*offset);

    /* no, get from log */
    return chk_aux_buf(offset,length,err_stream);
    }
/* display function for replay recovery monitoring */
static void replay_monitor(vmaddr,length,vmdata_ptr,vmdata_offset,
                           rec_hdr,i,msg)
    rvm_length_t    vmaddr;             /* base address of monitored range */
    rvm_length_t    length;             /* length of monitored range */
    char            *vmdata_ptr;        /* vmaddr of data if not null */
    rvm_offset_t    *vmdata_offset;     /* data offset in log if not null */
    rec_hdr_t       *rec_hdr;           /* ptr to record header if not null */
    rvm_length_t    i;                  /* index of monitor range matched */
    char            *msg;               /* ptr to message test if not null */
    {
    rvm_offset_t    pr_offset;          /* data vmaddr as offset */
    rvm_offset_t    data_offset;        /* offset of printed data*/
    rvm_length_t    data_length;        /* length of printed data */
    rvm_offset_t    limit_offset;       /* last valid data offset */
    rvm_length_t    count;              /* number of memory items to print */
    rvm_length_t    temp;
    unsigned long   save_num_format = num_format;
    long            save_num_count = num_count;
    rvm_bool_t      save_num_all = num_all;
    
    /* print msg, append rec_num, range num if specified */
    if (rec_hdr != NULL)
        fprintf(monitor_out,"%s record %ld, range %ld\n",msg,
                rec_hdr->rec_num,((nv_range_t *)rec_hdr)->range_num);
    else
        fprintf(monitor_out,"%s\n",msg);

    /* print monitor range number and sub-range actually matched */
    fprintf(monitor_out,"  monitored range %ld, ",i+1);
    pr_ulong(monitor_out,(rvm_length_t)rvm_chk_vec[i].vmaddr,
             rvm_chk_vec[i].radix,NULL);
    fprintf(monitor_out,", length %ld matched by\n",rvm_chk_vec[i].length);
    fprintf(monitor_out,"  modified range ");
    pr_ulong(monitor_out,vmaddr,rvm_chk_vec[i].radix,NULL);
    fprintf(monitor_out,", length %ld\n",length);

    /* see if data was specified */
    if ((vmdata_ptr == NULL)
        && ((vmdata_offset == NULL) || RVM_OFFSET_EQL_ZERO(*vmdata_offset)))
        goto exit;
    /* locate modification data and prepare for printing */
    if (vmaddr >= (rvm_length_t)rvm_chk_vec[i].vmaddr)
        {
        temp = vmaddr - (rvm_length_t)rvm_chk_vec[i].vmaddr;
        pr_offset = RVM_MK_OFFSET(0,vmaddr+temp);
        if (vmdata_ptr != NULL)
            {                               /* data in vm */
            data_offset = RVM_MK_OFFSET(0,vmdata_ptr);
            monitor_vm = rvm_true;
            }
        else
            {                           /* data in log */  
            data_offset = *vmdata_offset;
            monitor_vm = rvm_false;
            }
        data_length = ((rvm_length_t)rvm_chk_vec[i].vmaddr +
                       rvm_chk_vec[i].length) - vmaddr;
        if (data_length > length) data_length = length;
        }
    else
        {
        temp = (rvm_length_t)rvm_chk_vec[i].vmaddr - vmaddr;
        pr_offset = RVM_MK_OFFSET(0,rvm_chk_vec[i].vmaddr);
        if (vmdata_ptr != NULL)
            {                               /* data in vm */
            data_offset = RVM_MK_OFFSET(0,vmdata_ptr);
            monitor_vm = rvm_true;
            }
        else
            {                           /* data in log */  
            data_offset = *vmdata_offset;
            monitor_vm = rvm_false;
            }
        data_offset = RVM_ADD_LENGTH_TO_OFFSET(data_offset,temp);
        data_length = (vmaddr+length)-(rvm_length_t)rvm_chk_vec[i].vmaddr;
        if (data_length > rvm_chk_vec[i].length)
            data_length = rvm_chk_vec[i].length;
        }

    temp = num_format_size(rvm_chk_vec[i].format);
    count = (data_length+temp-1)/temp;
    limit_offset = RVM_ADD_LENGTH_TO_OFFSET(data_offset,data_length);
    /* print modification data */
    num_all = rvm_false;
    num_format = rvm_chk_vec[i].format;
    if (!pr_data_range(monitor_out,monitor_err,2,LINE_WIDTH,&data_offset,
                       &pr_offset,rvm_chk_vec[i].radix,count,chk_monitor,
                       &limit_offset,DATA_END_STR))
        {
        /* print error: turn off monitoring, no further updates allowed */
        rvm_chk_len = 0;
        rvm_no_update = rvm_true;
        }
  exit:
    putc('\n',monitor_out);
    num_format = save_num_format;
    num_count = save_num_count;
    num_all = save_num_all;
    }
/* monitor command support */
static rvm_bool_t do_monitor()
    {
    char            *cmd_save;
    long            key;
    char            str_name_buf[STR_NAME_LEN+1]; /* name buffer */
    unsigned long   save_num_format = num_format;
    long            save_num_count = num_count;
    rvm_bool_t      did_clear = rvm_false;
    rvm_bool_t      need_prompt = rvm_true;
    rvm_bool_t      save_num_all = num_all;
    rvm_bool_t      bool_ret = rvm_true;

    /* scan key word parameters */
    DO_FOREVER
        {
        skip_white(&cmd_cur);
        cmd_save = cmd_cur;
        key = scan_str_name(str_name_buf,monitor_key_vec,NULL);
        if (key == (long)UNKNOWN)
            {
            cmd_cur = cmd_save;
            break;
            }
        switch ((long)monitor_key_vec[key].target)
            {
          case CLEAR_KEY:
            clear_monitor(); did_clear = rvm_true; break;
          case NO_UPDATE_KEY:
            rvm_no_update = rvm_true; break;
          case UPDATE_KEY:
            rvm_no_update = rvm_false; break;
          default:
            bad_key_word(str_name_buf,NOT_MEANINGFUL_STR,stderr);
            goto err_exit;
            }
        }

    /* if no ranges, exit */
    if (scan_stop(*cmd_cur) && did_clear)
        goto exit;

    /* scan ranges */
    init_num_format();
    DO_FOREVER
        {
        /* check if at end or need to read new line */
        skip_white(&cmd_cur);
        if (scan_stop(*cmd_cur))
            {
            if (need_prompt)
                {
                if (!get_ans("  More ranges",rvm_false)) break;
                printf("\n  Enter list of addresses/format, ");
                printf("terminate with null line\n");
                need_prompt = rvm_false;
                }
            read_prompt_line(stdout,":",rvm_false);

            skip_white(&cmd_cur);
            if (scan_stop(*cmd_cur)) break; /* end of input */
            }

        /* reallocate monitor range vector if necessary */
        if ((rvm_chk_vec == NULL) || (rvm_chk_len == chk_alloc_len))
            {
            chk_alloc_len += 5;
            rvm_chk_vec = (chk_vec_t *)REALLOC(rvm_chk_vec,
                                    chk_alloc_len*sizeof(chk_vec_t));
            if (rvm_chk_vec == NULL)
                {
                rvm_chk_len = 0;
                fprintf(stderr,
                        "\n? Cannot allocate monitor range vector\n");
                goto err_exit;
                }
            }

        /* scan monitoring vmaddr */
        if (isdigit(*cmd_cur) || (*cmd_cur == 'x') || (*cmd_cur == 'X'))
            rvm_chk_vec[rvm_chk_len].vmaddr = (void *)str2ul(cmd_cur,
                            &cmd_cur,&rvm_chk_vec[rvm_chk_len].radix);
        else
            {
            fprintf(stderr,"\n? Monitoring address must be specified\n");
            goto err_exit;
            }

        /* scan range length & format */
        skip_white(&cmd_cur);
        if (*cmd_cur == '/')
            if (!scan_num_format())
                {
                bad_num_format(stderr);
                goto err_exit;
                }
        if (num_all)
            {
            fprintf(stderr,"\n? * not valid in monitor ranges ");
            fprintf(stderr,"- fixed length must be specified\n");
            goto err_exit;
            }
        rvm_chk_vec[rvm_chk_len].length =
                                num_count*num_format_size(num_format);
        rvm_chk_vec[rvm_chk_len].format = num_format;

        rvm_chk_len++;                  /* count ranges */
        }
    goto exit;

err_exit:
    bool_ret = rvm_false;
exit:
    num_format = save_num_format;
    num_count = save_num_count;
    num_all = save_num_all;
    return bool_ret;
    }
/* log status block commands */
static rvm_bool_t do_read_status()
    {
    rvm_return_t    retval;

    if (no_log()) return rvm_false;

    if ((retval=read_log_status(log,status_io)) != RVM_SUCCESS)
        {
        pr_rvm_error(stderr,retval,READ_STATUS_STR);
        return rvm_false;
        }
    return rvm_true;
    }

static rvm_bool_t do_write_status()
    {
    rvm_return_t    retval;

    if (no_log()) return rvm_false;

    if ((retval=write_log_status(log,NULL)) != RVM_SUCCESS)
        {
        pr_rvm_error(stderr,retval,WRITE_STATUS_STR);
        return rvm_false;
        }
    return rvm_true;
    }
/* open_log command support */
#define MAX_OPEN_KEYS   20              /* maximum number of key words */

static str_name_entry_t open_key_vec[MAX_OPEN_KEYS] =
                    {
                    {"file",(char *)FILE_KEY},
                    {"no_tail",(char *)NO_TAIL_KEY},
                    {"tail",(char *)TAIL_KEY},
                    {"no_update",(char *)NO_UPDATE_KEY},
                    {"update",(char *)UPDATE_KEY},
                    {"",NULL}           /* end mark, do not delete */
                    };

static rvm_bool_t do_open_log()
{
    log_dev_status_t *log_dev_status = (log_dev_status_t *)status_io;
    rvm_return_t    retval;             /* rvm return code */
    char            *cmd_save;          /* cmd line postion save */
    char            str_name_buf[STR_NAME_LEN+1]; /* name buffer */
    long            key;
    rvm_bool_t      no_tail = rvm_false; /* default: locate the tail */

    if (default_log != NULL)            /* see if log already open */
        {
        fprintf(stderr,"? Log file \"%s\" is already open\n",
                default_log->dev.name);
        return rvm_false;
        }

    /* scan key word parameters */
    DO_FOREVER
        {
        skip_white(&cmd_cur);
        cmd_save = cmd_cur;
        key = scan_str_name(str_name_buf,open_key_vec,NULL);
        if (key == (long)UNKNOWN)
            {
            cmd_cur = cmd_save;
            goto file_name;
            }
        switch ((long)open_key_vec[key].target)
            {
          case NO_UPDATE_KEY: rvm_no_update = rvm_true; break;
          case UPDATE_KEY:  rvm_no_update = rvm_false; break;
          case TAIL_KEY:    no_tail = rvm_false; break;
          case NO_TAIL_KEY: no_tail = rvm_true; break;
          case FILE_KEY:    goto file_name;
          default:
            bad_key_word(str_name_buf,NOT_MEANINGFUL_STR,stderr);
            return rvm_false;
            }
        }
    /* get name of log file or device */
file_name:
    cmd_save = cmd_cur;
    read_prompt_line(stdout,LOG_NAME_PROMPT,rvm_false);
    (void)scan_str(dev_str,MAXPATHLEN);

    /* build internal log structure and open log */
    if ((retval=open_log(dev_str,&log,status_io,NULL)) != RVM_SUCCESS)
        switch (retval)
            {
          case RVM_EVERSION_SKEW:
                {
                fprintf(stderr,"? Version skew\n");
                printf("\n  Log built with:  %s\n", log_dev_status->version);
                printf("  rvmutl built with: %s\n",RVM_VERSION);
                if (! get_ans("Do you want to continue anyway", rvm_false))
                    return do_quit();
                printf("\nWARNING: due to version skew, ");
		printf("rvmutl may not work correctly.\n");
                break;
                }

          case RVM_EIO:
                switch (errno)
                {
              case ENOENT:              /* non-existant file/directory/device */
                fprintf(stderr,
                        "? File or directory \"%s\" non-existant\n",
                        dev_str);
                if (get_ans("Do you want to create it",rvm_false))
                    {
                    cmd_cur = cmd_save;
                    return do_init_log();
                    }
                return rvm_false;
              default:                  /* unknown error */
                    {
                    perror("? I/O error opening log");
                    return rvm_false;
                    }
                }
          case RVM_ELOG:
                {
                fprintf(stderr,
                        "? Log status area damaged or uninitialized\n");
                if (get_ans("Do you want to initialize log",rvm_true))
                    return do_init_log();
                if (!get_ans("Do you want to continue anyway",rvm_false))
                    return rvm_false;
                printf("\nWARNING: due to status area damage, ");
		printf("rvmutl may not work correctly.\n");
                break;
                }
          default:
                {
                fprintf(stderr,"? Internal error, cannot open log, ");
		fprintf(stderr,"return code: %s\n",rvm_return(retval));
                return rvm_false;
                }
            }

    /* set pointers and status */
    log_dev = &log->dev;
    log_buf = &log->log_buf;
    status = &log->status;
    save_prev_log_tail = status->prev_log_tail;
    log->trunc_thread = cthread_self();

    /* locate tail if requested */
    if (!no_tail)
        return find_tail(stdout,stderr);

    return rvm_true;
}
/* close log command support */
static rvm_bool_t do_close_log()
    {
    rvm_return_t    retval;

    if (no_log()) return rvm_false;

    if ((retval=close_log(log)) != RVM_SUCCESS)
        {
        if (retval != RVM_EIO)
            fprintf(stderr,"? Error in closing log, return code: %s\n",
                    rvm_return(retval));
        else
            perror("? I/O error closing log");
        return rvm_false;
        }
    
    log = NULL;
    status = NULL;
    log_dev = NULL;
    log_buf = NULL;
    clear_monitor();
    return rvm_true;
    }
/* deallocation of change trees after recovery aborted */
static void kill_tree(tree)
    tree_root_t     *tree;
    {
    dev_region_t    *node;

    UNLINK_NODES_OF(tree,dev_region_t,node)
        free_dev_region(node);

    }

static void clear_cut()
    {
    int             i;

    for (i=0; i<log->seg_dict_len; i++)
        {
        kill_tree(&log->seg_dict_vec[i].mod_tree);
        assert(log->seg_dict_vec[i].mod_tree.root == NULL);
        }
    }
/* log truncation/recovery support */
#define MAX_RECOVER_KEYS   20           /* maximum number of key words */

static str_name_entry_t recover_key_vec[MAX_RECOVER_KEYS] =
                    {
                    {"clear",(char *)CLEAR_KEY},
                    {"file",(char *)FILE_KEY},
                    {"no_update",(char *)NO_UPDATE_KEY},
                    {"update",(char *)UPDATE_KEY},
                    {"",NULL}           /* end mark, do not delete */
                    };

static rvm_bool_t do_recover()
    {
    char            str_name_buf[STR_NAME_LEN+1]; /* name buffer */
    char            *cmd_save;
    long            key;
    rvm_return_t    retval;
    rvm_bool_t      bool_ret = rvm_true;

    /* get redirected monitor output */
    redirect_monitor();

    /* scan key word parameters */
    DO_FOREVER
        {
        skip_white(&cmd_cur);
        cmd_save = cmd_cur;
        key = scan_str_name(str_name_buf,recover_key_vec,NULL);
        if (key == (long)UNKNOWN)
            {
            cmd_cur = cmd_save;
            goto file_name;
            }
        switch ((unsigned long)recover_key_vec[key].target)
            {
          case CLEAR_KEY:
            clear_monitor(); break;
          case NO_UPDATE_KEY:
            rvm_no_update = rvm_true; break;
          case UPDATE_KEY:
            rvm_no_update = rvm_false; break;
          case FILE_KEY:
            goto file_name;
          default:
            bad_key_word(str_name_buf,NOT_MEANINGFUL_STR,stderr);
            goto err_exit;
            }
        }
    /* see if must open log_file */
file_name:
    skip_white(&cmd_cur);
    if (log == NULL)
        {
        if (scan_stop(*cmd_cur))
            {
            /* if no file specified, see what to do */
            (void)no_log();
            if (!get_ans("Do you want to open a log",rvm_true))
                goto err_exit;
            }
        if (!do_open_log()) goto err_exit;
        }
    else
        if (!scan_stop(*cmd_cur))
            {
            fprintf(stderr,"? Log file is already open\n");
            goto err_exit;
            }

    /* do recovery according to parameters and replay instructions */
    log->in_recovery = rvm_true;
    log->trunc_thread = (cthread_t)NULL;
    if ((retval=log_recover(log,&log->status.tot_recovery,
                            rvm_false,RVM_TRUNCATE_CALL))
        != RVM_SUCCESS)
        {
        pr_rvm_error(stderr,retval,"in recovery");
        goto err_exit;
        }

    /* check if recovery interrutped */
    if (chk_sigint(stdout))
        {                               /* yes */
        if (log_buf->ptr != -1)
            set_log_position();
        else no_rec = rvm_true;
        clear_cut();                    /* deallocate remaining change trees */
        goto sig_exit;
        }
    goto exit;                          /* no */
err_exit:
    bool_ret = rvm_false;
sig_exit:
    /* get clean status block since replay may have changed things */
    if (log && (retval=read_log_status(log,status_io)) != RVM_SUCCESS)
    {
	pr_rvm_error(stderr,retval,READ_STATUS_STR);
	bool_ret = rvm_false;
    }
exit:
    close_monitor();
    if (log)
	log->trunc_thread = cthread_self();
    return bool_ret;
    }
/* scan record number and locate */
static rvm_bool_t find_rec_num(offset,rec_hdr)
    rvm_offset_t    *offset;
    rec_hdr_t       **rec_hdr;
    {
    rvm_length_t    rec_num;
    rvm_return_t    retval;

    /* be sure we have a log */
    if (no_log()) return rvm_false;

    /* scan record number & locate */
    skip_white(&cmd_cur);
    rec_num = str2ul(cmd_cur,&cmd_cur,NULL);
    if (rec_num == 0)
        {
        fprintf(stderr,"\n? Record number required\n");
        return rvm_false;
        }
    if ((retval=locate_rec(rec_num,NULL)) != RVM_SUCCESS)
        {
        pr_rvm_error(stderr,retval,"scanning log file");
        no_rec = rvm_true;
        return rvm_false;
        }
    if (chk_sigint(stdout)) return rvm_true;
    if (log_buf->ptr == -1)
        {
        no_rec = rvm_true;
        fprintf(stderr,"\n? Record not found\n");
        return rvm_false;
        }

    /* return address and offset of record header */
    *rec_hdr = set_log_position();
    *offset = RVM_ADD_LENGTH_TO_OFFSET(log_buf->offset,log_buf->ptr);
    return rvm_true;
    }
/* recovery replay command & support */
#define MAX_REPLAY_KEYS   20            /* maximum number of key words */

static str_name_entry_t replay_key_vec[MAX_REPLAY_KEYS] =
                    {
                    {"clear",(char *)CLEAR_KEY},
                    {"file",(char *)FILE_KEY},
                    {"head",(char *)HEAD_KEY},
                    {"tail",(char *)TAIL_KEY},
                    {"no_update",(char *)NO_UPDATE_KEY},
                    {"update",(char *)UPDATE_KEY},
                    {"",NULL}           /* end mark, do not delete */
                    };

static rvm_bool_t do_replay()
    {
    char            str_name_buf[STR_NAME_LEN+1]; /* name buffer */
    char            *cmd_save;
    long            key;
    rec_hdr_t       *rec_hdr;
    rvm_return_t    retval;
    rvm_bool_t      bool_ret = rvm_true;

    /* scan key word parameters */
    DO_FOREVER
        {
        skip_white(&cmd_cur);
        cmd_save = cmd_cur;
        key = scan_str_name(str_name_buf,replay_key_vec,NULL);
        if (key == (long)UNKNOWN)
            {
            cmd_cur = cmd_save;         /* assume it's a file name */
            goto file_name;
            }
        rvm_replay = rvm_true;
        switch ((long)replay_key_vec[key].target)
            {
          case CLEAR_KEY:
            clear_monitor(); break;
          case NO_UPDATE_KEY:
            rvm_no_update = rvm_true; break;
          case UPDATE_KEY:
            rvm_no_update = rvm_false; break;
          case HEAD_KEY:
            if (!find_rec_num(&status->log_head,&rec_hdr))
                goto err_exit;
            status->first_rec_num = rec_hdr->rec_num;
            status->prev_trunc = rec_hdr->timestamp;
            break;
          case TAIL_KEY:
            if (!find_rec_num(&status->log_tail,&rec_hdr))
                goto err_exit;
            switch (rec_hdr->struct_id)
                {
              case trans_hdr_id:
              case log_seg_id:
		  /* retval=load_sub_rec(rec_hdr->rec_length,cur_direction); */
                retval=load_sub_rec(rec_hdr->rec_length,FORWARD);
                if (retval != RVM_SUCCESS)
                    {
                    pr_rvm_error(stderr,retval,"reading log");
                    goto err_exit;
                    }
                rec_hdr = (rec_hdr_t *)&log_buf->buf[log_buf->ptr];
                assert(rec_hdr->struct_id == rec_end_id);
              case log_wrap_id: break;
              default: assert(rvm_false);
                }
            status->last_rec_num = rec_hdr->rec_num;
            status->last_trunc = rec_hdr->timestamp;
            break;
          case FILE_KEY:
            goto file_name;
          default:
            bad_key_word(str_name_buf,NOT_MEANINGFUL_STR,stderr);
            goto err_exit;
            }
        if (chk_sigint(NULL)) goto sig_exit;;
        }

file_name:
    if (chk_sigint(NULL)) goto sig_exit;
    bool_ret = do_recover();
    goto sig_exit;
err_exit:
    bool_ret = rvm_false;
sig_exit:
    /* get clean status block since things may have changed */
    if (log != NULL)
        if ((retval=read_log_status(log,status_io)) != RVM_SUCCESS)
            {
            pr_rvm_error(stderr,retval,READ_STATUS_STR);
            bool_ret = rvm_false;
            }
    rvm_replay = rvm_false;
    return bool_ret;
    }

#ifdef UNUSED_FUNCTIONS
/* re-write log buffer contents */
static rvm_bool_t do_write_log_buf()
    {
    long            retval;

    if (no_log()) return rvm_false;

    retval = write_dev(log_dev,&log_buf->offset,log_buf->buf,
                       log_buf->r_length,SYNCH);
    if (retval < 0)
        {
        pr_rvm_error(stderr,RVM_EIO,"writing log");
        return rvm_false;
        }

    return rvm_true;
    }
#endif
/* copy log file/partition to file/partition */
static rvm_bool_t do_copy_log()
    {
    device_t        dest_dev;           /* device descriptor for copy */
    log_status_t    save_status;        /* source log status save area */
    rvm_offset_t    src_offset;
    rvm_offset_t    dest_offset;
    rvm_return_t    retval;
    rvm_bool_t      cmd_retval = rvm_true;

    /* see if log already open */
    if (default_log != NULL)
        {
        printf("Log file \"%s\" is already open\n",
                default_log->dev.name);
        if (!get_ans("Do you want to use it for the source log",rvm_true))
            if (!do_close_log()) return rvm_false;
        }

    /* get name of source log file or device */
    if (default_log == NULL)
        if (!do_open_log()) return rvm_false;
    assert(default_log != NULL);

    /* get name of destination file/device */
    DO_FOREVER
        {
        read_prompt_line(stdout,
                         "Enter name of destination file or device:",
                         rvm_false);
        (void)scan_str(dev_str,MAXPATHLEN);
        if (strcmp(default_log->dev.name,dev_str) != 0)
            break;                      /* not same as source, go ahead */
        }

    /* open destination device */
    if ((retval=dev_init(&dest_dev,dev_str)) != RVM_SUCCESS)
        {
        pr_rvm_error(stderr,retval,"getting destination full path name");
        return rvm_false;
        }
    if (open_dev(&dest_dev,O_WRONLY | O_CREAT | O_TRUNC,mode) < 0)
        {
        pr_rvm_error(stderr,RVM_EIO,"opening destination file/device");
        goto err_exit;
        }
    if (set_dev_char(&dest_dev,NULL) < 0)
        {
        pr_rvm_error(stderr,RVM_EIO,
                     "setting destination file/device characteristics");
        goto err_exit;
        }
    /* print source and destination devices */
    printf("\n  Copying from %s to %s\n\n",default_log->dev.name,
           dev_str);

    /* set up destination status block */
    (void)BCOPY(status,&save_status,sizeof(log_status_t));
    if ((!log->dev.raw_io) && (dest_dev.raw_io))
        {                               /* convert from file to partition */
        dest_dev.num_bytes =
            RVM_SUB_LENGTH_FROM_OFFSET(log_dev->num_bytes,
                                       FILE_STATUS_OFFSET);
        dest_dev.num_bytes =
            RVM_ADD_LENGTH_TO_OFFSET(dest_dev.num_bytes,
                                     RAW_STATUS_OFFSET);
        status->log_start =
            RVM_MK_OFFSET(0,RAW_STATUS_OFFSET+LOG_DEV_STATUS_SIZE);
        status->log_head =
            RVM_SUB_LENGTH_FROM_OFFSET(status->log_head,
                                       FILE_STATUS_OFFSET);
        status->log_head =
            RVM_ADD_LENGTH_TO_OFFSET(status->log_head,
                                     RAW_STATUS_OFFSET);
        status->log_tail =
            RVM_SUB_LENGTH_FROM_OFFSET(status->log_tail,
                                       FILE_STATUS_OFFSET);
        status->log_tail =
            RVM_ADD_LENGTH_TO_OFFSET(status->log_tail,
                                     RAW_STATUS_OFFSET);
        if (!RVM_OFFSET_EQL_ZERO(status->prev_log_head))
            {
            status->prev_log_head =
                RVM_SUB_LENGTH_FROM_OFFSET(status->prev_log_head,
                                           FILE_STATUS_OFFSET);
            status->prev_log_head =
                RVM_ADD_LENGTH_TO_OFFSET(status->prev_log_head,
                                         RAW_STATUS_OFFSET);
            }
        if (!RVM_OFFSET_EQL_ZERO(status->prev_log_tail))
            {
            status->prev_log_tail =
                RVM_SUB_LENGTH_FROM_OFFSET(status->prev_log_tail,
                                           FILE_STATUS_OFFSET);
            status->prev_log_tail =
                RVM_ADD_LENGTH_TO_OFFSET(status->prev_log_tail,
                                         RAW_STATUS_OFFSET);
            }
        }
    /* convert from partition to file */
    else if ((log->dev.raw_io) && (!dest_dev.raw_io))
        {
        dest_dev.num_bytes =
            RVM_SUB_LENGTH_FROM_OFFSET(log_dev->num_bytes,
                                       RAW_STATUS_OFFSET);
        dest_dev.num_bytes =
            RVM_ADD_LENGTH_TO_OFFSET(dest_dev.num_bytes,
                                     FILE_STATUS_OFFSET);
        status->log_start =
            RVM_MK_OFFSET(0,FILE_STATUS_OFFSET+LOG_DEV_STATUS_SIZE);
        status->log_head =
            RVM_SUB_LENGTH_FROM_OFFSET(status->log_head,
                                       RAW_STATUS_OFFSET);
        status->log_head =
            RVM_ADD_LENGTH_TO_OFFSET(status->log_head,
                                     FILE_STATUS_OFFSET);
        status->log_tail =
            RVM_SUB_LENGTH_FROM_OFFSET(status->log_tail,
                                       RAW_STATUS_OFFSET);
        status->log_tail =
            RVM_ADD_LENGTH_TO_OFFSET(status->log_tail,
                                     FILE_STATUS_OFFSET);
        if (!RVM_OFFSET_EQL_ZERO(status->prev_log_head))
            {
            status->prev_log_head =
                RVM_SUB_LENGTH_FROM_OFFSET(status->prev_log_head,
                                           RAW_STATUS_OFFSET);
            status->prev_log_head =
                RVM_ADD_LENGTH_TO_OFFSET(status->prev_log_head,
                                         FILE_STATUS_OFFSET);
            }
        if (!RVM_OFFSET_EQL_ZERO(status->prev_log_tail))
            {
            status->prev_log_tail =
                RVM_SUB_LENGTH_FROM_OFFSET(status->prev_log_tail,
                                           RAW_STATUS_OFFSET);
            status->prev_log_tail =
                RVM_ADD_LENGTH_TO_OFFSET(status->prev_log_tail,
                                         FILE_STATUS_OFFSET);
            }
        }
    else
        /* no conversions needed */
        dest_dev.num_bytes = log_dev->num_bytes;

    /* copy modified status block to new log file/partition */
    if ((retval=write_log_status(log,&dest_dev)) != RVM_SUCCESS)
        {
        pr_rvm_error(stderr,retval,"writing destination log status block");
        goto err_exit;
        }

    /* restore source log status */
    dest_offset = status->log_start;
    (void)BCOPY(&save_status,status,sizeof(log_status_t));

    /* copy log data area to new log */
    src_offset = status->log_start;
    while (!RVM_OFFSET_EQL(src_offset,log->dev.num_bytes))
        {
        retval = init_buffer(log,&src_offset,FORWARD,NO_SYNCH);
        if (retval != RVM_SUCCESS)
            {
            pr_rvm_error(stderr,retval,"reading source log");
            goto err_exit;
            }
        if (write_dev(&dest_dev,&dest_offset,log_buf->buf,
                              log_buf->r_length,NO_SYNCH) < 0)
            {
            pr_rvm_error(stderr,RVM_EIO,"writing log copy");
            goto err_exit;
            }
        src_offset = RVM_ADD_LENGTH_TO_OFFSET(src_offset,
                                              log_buf->r_length);
        dest_offset = RVM_ADD_LENGTH_TO_OFFSET(dest_offset,
                                               log_buf->r_length);
        }
    goto exit;
err_exit:
    cmd_retval = rvm_false;
exit:
    /* close destination device */
    if (close_dev(&dest_dev) < 0)
        pr_rvm_error(stderr,RVM_EIO,"closing log copy");

    if (dest_dev.name != NULL)
        free(dest_dev.name);

    return cmd_retval;
    }
/* quit, update, no_update command and signal support */
rvm_bool_t do_quit()
    {
    int             err = 0;

    /* close peek/poke device */
    close_peekpoke_dev();

    /* close log file/device */
    if (log != NULL)
        {
        eof = close_dev(log_dev);
        if (eof != 0)
            {
            err = errno;
            perror("? Could not close log");
            return rvm_false;
            }
        }

    /* close open segments */

    if (err != 0) exit(1);
    else exit(0);
    return rvm_true;
    }

static rvm_bool_t do_update()
    {
    rvm_no_update = rvm_false;
    return rvm_true;
    }

static rvm_bool_t do_no_update()
    {
    return rvm_no_update = rvm_true;
    }

static void sigint_handler()
    {
    stop_sw = rvm_true;
    }

rvm_bool_t chk_sigint(out_stream)
    FILE            *out_stream;
    {
    if ((out_stream != NULL) && stop_sw)
        fprintf(out_stream," -- stopped\n");
    return stop_sw;
    }
/* command dispatch vector */

#define MAX_CMDS    64                  /* maximum number of cmds */

static str_name_entry_t  cmd_vec[MAX_CMDS] = /* command dispatch vector */
                    {
		    {"all_spec",NULL,do_show_all_spec},
                    {"seg_dict",NULL,do_build_seg_dict},
                    {"build_seg_dict",NULL,do_build_seg_dict},
                    {"close_log",NULL,do_close_log},
                    {"copy_log",NULL,do_copy_log},
                    {"earliest",NULL,do_earliest},
                    {"init_log",NULL,do_init_log},
                    {"find_earliest",NULL,do_find_earliest},
		    {"find_hole",NULL,do_find_hole},
                    {"find_tail",NULL,do_find_tail},
                    {"head",NULL,do_head},
                    {"log",NULL,do_open_log},
                    {"open_log",NULL,do_open_log},
                    {"n",NULL,do_next,STICKY},
                    {"next",NULL,do_next,STICKY},
                    {"ns",NULL,do_next_sub_rec,STICKY},
                    {"next_sub_rec",NULL,do_next_sub_rec,STICKY},
                    {"p",NULL,do_peek,STICKY},
                    {"pr",NULL,do_prev,STICKY},
                    {"prev",NULL,do_prev,STICKY},
                    {"ps",NULL,do_prev_sub_rec,STICKY},
                    {"prev_sub_rec",NULL,do_prev_sub_rec,STICKY},
                    {"peek",NULL,do_peek,STICKY},
                    {"poke",NULL,do_poke},
                    {"quit",NULL,do_quit},
                    {"read_status",NULL,do_read_status},
                    {"monitor",NULL,do_monitor},
                    {"replay",NULL,do_replay},
                    {"recover",NULL,do_recover},
                    {"set",NULL,do_set},
                    {"s",NULL,do_show,STICKY},
                    {"show",NULL,do_show,STICKY},
                    {"sizeof",NULL,do_sizeof},
                    {"status",NULL,do_show_log_status},
                    {"log_status",NULL,do_show_log_status},
                    {"sr",NULL,do_sub_rec,STICKY},
                    {"sub_rec",NULL,do_sub_rec,STICKY},
                    {"statistics",NULL,do_show_statistics},
                    {"tail",NULL,do_tail},
                    {"update",NULL,do_update},
                    {"no_update",NULL,do_no_update},
                    {"write_status",NULL,do_write_status},
                    {"",NULL}           /* end mark, do not delete */
                    };
/* command name scan and dispatch */
static void do_commands(new_file)
    char            *new_file;        /* new input file */
    {
    char            cmd_name[STR_NAME_LEN+1]; /* command name */
    long            cmd_index;          /* index of cmd in cmd vector */
    char            in_file[MAXPATHLEN+1]; /* input file name */
    FILE            *old_stream;        /* previous in_stream */
    rvm_bool_t      sticky = rvm_false; /* if true, last cmd was sticky */

    /* set up current input stream */
    old_stream = in_stream;
    if (new_file == NULL)
        {
        strcpy(in_file,"stdin");
        in_stream = stdin;
        }
    else
        {
        scan_str(in_file,MAXPATHLEN);
        in_stream = fopen(in_file,"r");
        if (in_stream == NULL)
            {
            fprintf(stderr,"\n? Cannot open input file %s",in_file);
            perror(NULL);
            in_stream = old_stream;
            return;
            }
        }

    /* command loop */
    DO_FOREVER
        {
        /* get command line & handle EOF, errors, ^C */
        if (read_prompt_line(stdout,"*",rvm_false) == NULL)
            {
            if (chk_sigint(NULL))
                stop_sw = rvm_false;
            else
                if (feof(in_stream) == 0)
                    {                   /* i/o error */
                    fprintf(stderr,"\n? Error reading %s",in_file );
                    perror(NULL);
                    }

            /* close redirect stream */
            if (in_stream == stdin) continue;
            fclose(in_stream);
            in_stream = old_stream;
            return;
            }
        skip_white(&cmd_cur);

        /* ignore comment lines */
        if (*cmd_cur == '#')
            {
            *cmd_cur = '\0';
            continue;
            }

        /* check for input redirection */
        if (*cmd_cur == '<')
            {
            incr_cur(1);
            skip_white(&cmd_cur);
            do_commands(cmd_cur);
            cmd_line[0] = '\0';
            cmd_cur = cmd_line;
            sticky = is_sticky_line = rvm_false;
            continue;
            }

        /* check for sticky processing */
        if (end_line(*cmd_cur))
            {
            if (!sticky) continue;
            cmd_cur = strcpy(cmd_line,sticky_buf);
            is_sticky_line = rvm_true;
            skip_white(&cmd_cur);
            }

        /* lookup command and execute */
        cmd_index = scan_str_name(cmd_name,cmd_vec,"command");
        if (cmd_index != (long)UNKNOWN)
            {
            if ((sticky=cmd_vec[cmd_index].sticky))
                (void)strcpy(sticky_buf,cmd_line);
            stop_sticky = rvm_false;
            if (!(cmd_vec[cmd_index].func)())
                {
                sticky = rvm_false;     /* shut off sticky processing */
                }
            if ((stop_sticky) || (stop_sw))
                sticky = rvm_false;     /* cmd or sigint stopped
					   sticky processing */
            is_sticky_line = rvm_false;
            stop_sw = rvm_false;
            }
        *cmd_cur = '\0';                /* force reading new line */
        }
    }

#ifdef UNUSED_FUNCTIONS
/* command line working directory switch function */
static rvm_bool_t do_wd_sw(argc,argv)
    int             argc;
    char            *argv[];
    {
    char            *wd;                /* working directory name ptr */

    wd = argv[args];
    if ((argc == args) || (wd[0] == '-'))
        wd = NULL;                      /* no parameter given */
    else args++;

    return rvm_true;
    }
#endif
/* command line switches */

#define MAX_CMD_SWS     32

static str_name_entry_t cmd_sw_vec[MAX_CMD_SWS] =
    {
        {"",NULL}                       /* end mark, do not delete */
    };
/* command line switch scanner */
static void do_cmd_switches(argc,argv)
    int             argc;
    char            *argv[];
    {
    char            *sw;
    long            sw_index;
    char            sw_str[MAXPATHLEN+1];

    while (argc > args)
        {
        sw = argv[args];
        if (sw[0] != '-')
            break;                      /* no more switches */

        /* see if switch name and '-' are seperated */
        if (strlen(sw) == 1)
            {
            args++;
            if (argc > args)
                {
                sw = strcpy(sw_str,"-");
                (void)strcat(sw,argv[args]);
                }
            else
                {
                fprintf(stderr,"\n?  '-':  switch not found\n");
                exit(1);
                }
            }
        else args++;

        /* lookup switch */
        sw_index = lookup_str_name(sw,cmd_sw_vec,"switch");
        if (sw_index == (long)UNKNOWN) exit(1); /* error */

        /* invoke processing function */
        (void)(cmd_sw_vec[sw_index].func)(argc,argv);
        }
    }

int
main(argc,argv)
    int             argc;
    char            *argv[];
    {
    rvm_return_t    retval;

    /* initialization */
    rvm_debug(0);
    rvm_utlsw = rvm_true;
    cmd_line[0] = '\0';
    cmd_cur = cmd_line;
    init_num_format();
    signal(SIGINT,sigint_handler);
#ifdef RVM_LOG_TAIL_BUG
    signal(SIGBUS,my_sigBus__FiT1P10sigcontext);
#endif
    /* process command line switches */
    args = 1;
    do_cmd_switches(argc,argv);

    /* initialize RVM */
    if ((retval=rvm_initialize(RVM_VERSION,NULL)) != RVM_SUCCESS)
        {
        fprintf(stderr,"? rvm_initialize failed, code: %s\n",
                rvm_return(retval));
        exit(1);
        }

    /* remaining initialization */
    rvm_chk_sigint = chk_sigint;
    rvm_monitor = replay_monitor;
    peekpoke.length = RECOVERY_BUF_LEN;
    peekpoke.buf = page_alloc(peekpoke.length);
    if (peekpoke.buf == NULL)
        {
        fprintf(stderr,"? internal buffer allocation failed\n");
        exit(1);
        }

    /* read log file/device name from command line */
    if (argc > args)
        {
        cmd_cur = argv[args++];
        do_open_log();                  /* open/init log */
        }

    /* accept commands */
    do_commands(NULL);                  /* never returns */

    return 0;                           /* just for lint... */
    }
