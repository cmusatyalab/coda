#ifndef _BLURB_
#define _BLURB_
/*

     RVM: an Experimental Recoverable Virtual Memory Package
			Release 1.3

       Copyright (c) 1990-1994 Carnegie Mellon University
                      All Rights Reserved.

Permission  to use, copy, modify and distribute this software and
its documentation is hereby granted (including for commercial  or
for-profit use), provided that both the copyright notice and this
permission  notice  appear  in  all  copies  of   the   software,
derivative  works or modified versions, and any portions thereof,
and that both notices appear  in  supporting  documentation,  and
that  credit  is  given  to  Carnegie  Mellon  University  in all
publications reporting on direct or indirect use of this code  or
its derivatives.

RVM  IS  AN  EXPERIMENTAL  SOFTWARE  PACKAGE AND IS KNOWN TO HAVE
BUGS, SOME OF WHICH MAY  HAVE  SERIOUS  CONSEQUENCES.    CARNEGIE
MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.
CARNEGIE MELLON DISCLAIMS ANY  LIABILITY  OF  ANY  KIND  FOR  ANY
DAMAGES  WHATSOEVER RESULTING DIRECTLY OR INDIRECTLY FROM THE USE
OF THIS SOFTWARE OR OF ANY DERIVATIVE WORK.

Carnegie Mellon encourages (but does not require) users  of  this
software to return any improvements or extensions that they make,
and to grant Carnegie Mellon the  rights  to  redistribute  these
changes  without  encumbrance.   Such improvements and extensions
should be returned to Software.Distribution@cs.cmu.edu.

*/

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/rvm-src/rvm/Attic/rvm_io.c,v 4.8 1998/03/06 20:21:43 braam Exp $";
#endif _BLURB_

/*
*
*                               RVM I/O
*
*/

#include <sys/file.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/errno.h>
#ifdef __MACH__
#include <sysent.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif
#include "rvm_private.h"

/* global variables */
device_t            *rvm_errdev;        /* last device to have error */
int                 rvm_ioerrno=0;      /* also save the errno for I/O error */
rvm_length_t        rvm_max_read_len = MAX_READ_LEN; /* maximum single read in Mach */

extern int          errno;              /* kernel error number */
extern char         *rvm_errmsg;        /* internal error message buffer */
extern log_t        *default_log;       /* log descriptor */
extern rvm_bool_t   rvm_utlsw;          /* operating under rvmutl */
extern rvm_bool_t   rvm_no_update;      /* no segment or log update if true */


/* static prototypes */
static rvm_bool_t in_wrt_buf(char *addr, rvm_length_t len);
static long chk_seek(device_t *dev, rvm_offset_t *offset);

#ifndef ZERO
#define ZERO 0
#endif

/* buffer address checks: test if [addr, addr+len]
   lies inside default_log->dev.wrt_buf */
static rvm_bool_t in_wrt_buf(char *addr, rvm_length_t len)
{
    char            *end_addr;
    char            *buf_end_addr;

    if (default_log == NULL) 
	return rvm_false;
    if (default_log->dev.wrt_buf == NULL) 
	return rvm_false;

    end_addr = RVM_ADD_LENGTH_TO_ADDR(addr,len);
    buf_end_addr = RVM_ADD_LENGTH_TO_ADDR(default_log->dev.wrt_buf,
                                          default_log->dev.wrt_buf_len);
    if (((addr >= default_log->dev.wrt_buf)
	  && (addr < buf_end_addr))
	&& ((end_addr > default_log->dev.wrt_buf)
	     && (end_addr <= buf_end_addr)))
        return rvm_true;

    return rvm_false;
}

/* seek to position if required: raw devices must 
   seek to a sector index. Sanity checks size of
   device against offset. */
static long chk_seek(device_t *dev, rvm_offset_t *offset)
{
    long            retval=0;

    /* raw i/o offset must be specified and sector aligned */
    ASSERT((dev->raw_io) ? (offset != NULL) : 1);
    ASSERT((dev->raw_io) ? (OFFSET_TO_SECTOR_INDEX(*offset) == 0) : 1);
    ASSERT(RVM_OFFSET_LEQ(dev->last_position,dev->num_bytes));

    /* seek if offset specified */
    if (offset != NULL) {
        ASSERT(RVM_OFFSET_EQL_ZERO(*offset) ? 1
               : RVM_OFFSET_LSS(*offset,dev->num_bytes));
        if (!RVM_OFFSET_EQL(dev->last_position,*offset)) {
            retval = lseek((int)dev->handle,
                           (off_t)RVM_OFFSET_TO_LENGTH(*offset),
                           L_SET);
            if (retval >= 0)
                dev->last_position = *offset;
	    else {
		rvm_errdev = dev;
		rvm_ioerrno = errno;
	    }
	}
    }
    return retval;
}

/* set device characteristics for device
   device descriptor mandatory, lenght optional */
long set_dev_char(device_t *dev, rvm_offset_t *dev_length)
{
    struct stat     statbuf;            /* status descriptor */
    long            retval;             /* return value */
    rvm_offset_t    temp;               /* offset calc. temp. */
    unsigned long   mode;               /* protection mode */

    errno = 0;

    /* get file or device status */
    retval = fstat(dev->handle,&statbuf);
    if (retval != 0) {
        rvm_errdev = dev;
	rvm_ioerrno = errno;
        return retval;
    }

    /* find type */
    mode = statbuf.st_mode & S_IFMT;
    dev->type = mode;
    switch (mode)
        {
      case S_IFCHR:                     /* note raw io */
        dev->raw_io = rvm_true;
        break;
	/* Linux doesn't have BSD style raw character devices.
	   However, one can write to the block device directly.
	   This takes care, since we must sync it as if we 
	   do file IO.  We use dev->type == S_IFBLK 
	   to achieve this. The result could be good, since the
	   buffer cache will flush the blocks to the disk more 
	   efficiently than individual synchronous writes would 
	   take place.
                     */
      case S_IFBLK:  
	dev->raw_io = rvm_true;
	break;
      case S_IFREG:
        dev->num_bytes = RVM_MK_OFFSET(0,
                             CHOP_TO_SECTOR_SIZE(statbuf.st_size));
        break;
      default:
        rvm_errdev = dev;
        return  -1;
        }

    /* set optional length of device or file */
    if (dev_length != NULL)
        {
        temp = CHOP_OFFSET_TO_SECTOR_SIZE(*dev_length);

        if (!RVM_OFFSET_EQL_ZERO(temp))
            if (RVM_OFFSET_GTR(dev->num_bytes,temp)
                || RVM_OFFSET_EQL_ZERO(dev->num_bytes))
                dev->num_bytes = temp;
        }

    return 0;
}
/* open device or file */
long open_dev(dev,flags,mode)
    device_t        *dev;               /* device descriptor */
    long            flags;              /* open option flags */
    long            mode;               /* create protection modes */
    {
    long            handle;             /* device handle returned */

    errno = 0;
    dev->handle = 0;

    /* attempt to open */
#ifdef DJGPP
    handle = (long)open(dev->name,flags | O_BINARY ,mode);
#else
    handle = (long)open(dev->name,flags ,mode);
#endif 
    if (handle < 0)
        {
        rvm_errdev = dev;
	rvm_ioerrno = errno;
        return handle;                  /* can't open, see errno... */
        }

    dev->handle = (long)handle;
    RVM_ZERO_OFFSET(dev->last_position);
    if (flags == O_RDONLY)
        dev->read_only = rvm_true;

    return 0;
    }
/* close device or file */
long close_dev(dev)
    device_t        *dev;               /* device descriptor */
    {
    long            retval;

    ASSERT(((dev == &default_log->dev) && (!rvm_utlsw)) ?
           (!LOCK_FREE(default_log->dev_lock)) : 1);

    errno = 0;
    if (dev->handle == 0) return 0;

    /* close device */
    if ((retval=close((int)dev->handle)) < 0)
	{
        rvm_errdev = dev;
	rvm_ioerrno = errno;
	}
    else
        dev->handle = 0;

    return retval;
    }
/* read bytes from device or file */
long read_dev(dev,offset,dest,length)
    device_t        *dev;               /* device descriptor */
    rvm_offset_t    *offset;            /* device offset */
    char            *dest;              /* address of data destination */
    rvm_length_t    length;             /* length of transfer */
    {
    rvm_offset_t    last_position;
    long            nbytes;
    long            read_len;
    long            retval;

    ASSERT(dev->handle != ZERO);
    ASSERT(length != 0);
    ASSERT((dev->raw_io) ? (SECTOR_INDEX(length) == 0) : 1);
    ASSERT(((dev == &default_log->dev) && (!rvm_utlsw)) ?
           (!LOCK_FREE(default_log->dev_lock)) : 1);

    /* seek if necessary */
    errno = 0;
    if ((retval = chk_seek(dev,offset)) < 0)
        return retval;
    last_position = RVM_ADD_LENGTH_TO_OFFSET(dev->last_position,
                                             length);
    ASSERT(RVM_OFFSET_EQL_ZERO(*offset) ? 1
           : RVM_OFFSET_LEQ(last_position,dev->num_bytes));

    /* do read in larg-ish blocks to avoid kernel buffer availability problems
       also zero region if /dev/null being read */
    retval = 0;
    while (length != 0)
        {
        if (length <= rvm_max_read_len) 
		read_len = length;
        else 
		read_len = rvm_max_read_len;
	nbytes=read((int)dev->handle,dest,(int)read_len);
        if (nbytes < 0) {
		rvm_errdev = dev;
		rvm_ioerrno = errno;
		return nbytes;
	}
        if (nbytes == 0)                /* force a cheap negative test */
            if (rvm_utlsw && dev->raw_io) /* since rarely used */
                if (!strcmp(dev->name,"/dev/null"))
                    {
                    retval = length;
                    BZERO(dest,length); /* zero the read region */
                    break;
                    }
        ASSERT((dev->raw_io) ? (nbytes == read_len) : 1);
        retval += nbytes;
        dest += nbytes;
        length -= nbytes;
        }

    /* update position */
    dev->last_position = RVM_ADD_LENGTH_TO_OFFSET(dev->last_position,
                                                  retval);
    return retval;
    }
/* write bytes to device or file */
long write_dev(dev,offset,src,length,sync)
    device_t        *dev;               /* device descriptor */
    rvm_offset_t    *offset;            /* device offset */
    char            *src;               /* address of data source */
    rvm_length_t    length;             /* length of transfer */
    rvm_bool_t      sync;               /* fsync if true */
{
    rvm_offset_t    last_position;
    long            retval;
    long            wrt_len = length;   /* for no_update mode */

    ASSERT(dev->handle != ZERO);
    ASSERT(length != 0);
    ASSERT((dev->raw_io) ? (SECTOR_INDEX(length) == 0) : 1);
    ASSERT(((dev == &default_log->dev) && (!rvm_utlsw)) ?
           (!LOCK_FREE(default_log->dev_lock)) : 1);

    /* seek if necessary */
    errno = 0;
    if ((retval = chk_seek(dev,offset)) < 0) 
	return retval;
    last_position = RVM_ADD_LENGTH_TO_OFFSET(dev->last_position,
                                             length);
    ASSERT(RVM_OFFSET_LEQ(last_position,dev->num_bytes));

    /* do write if not in no update mode */
    if (!(rvm_utlsw && rvm_no_update)) {
        if ((wrt_len=write((int)dev->handle,src,(int)length)) < 0) {
            rvm_errdev = dev;
	    rvm_ioerrno = errno;
            return wrt_len;
	}

        /* fsync if doing file i/o */
        if ( ((!dev->raw_io) && (sync==SYNCH)) ||
	     ((dev->raw_io) && (dev->type=S_IFBLK))) {
            if ((retval=fsync((int)dev->handle))  < 0) {
                rvm_errdev = dev;
		rvm_ioerrno = errno;
                return retval;
	    }
	}
    }

    /* update position (raw i/o must be exact) */
    ASSERT((dev->raw_io) ? (wrt_len == length) : 1);
    dev->last_position = RVM_ADD_LENGTH_TO_OFFSET(dev->last_position,
                                                  wrt_len);
    return wrt_len;
}
/* gather write for files */
static long gather_write_file(dev,offset,wrt_len)
    device_t        *dev;               /* device descriptor */
    rvm_offset_t    *offset;            /* disk position */
    rvm_length_t    *wrt_len;           /* num bytes written (out) */
    {
    long            retval;             /* kernel return value */
    long            iov_index = 0;      /* index of current iov entry */
    int             count;              /* iov count for Unix i/o */
 
    ASSERT(((dev == &default_log->dev) && (!rvm_utlsw)) ?
           (!LOCK_FREE(default_log->dev_lock)) : 1);

    /* seek if necessary */
    if ((retval = chk_seek(dev,offset)) < 0)
        return retval;

    /* do gather write in groups of 16 for Unix */
    if (!(rvm_utlsw && rvm_no_update))
        while (dev->iov_cnt != 0)
            {
            if (dev->iov_cnt > 16) count = 16;
            else count = dev->iov_cnt;

            retval = writev((int)dev->handle,
                            (struct iovec *)&dev->iov[iov_index],count);
            if (retval < 0)
                {
                rvm_errdev = dev;
		rvm_ioerrno = errno;
                return retval;
                }

            *wrt_len += (rvm_length_t)retval;
            dev->iov_cnt -= count;
            iov_index += count;
            }
    else
        /* sum lengths for no_update mode */
        for (count=0; count < dev->iov_cnt; count++)
            *wrt_len += dev->iov[count].length;


    /* update position */
    dev->last_position = RVM_ADD_LENGTH_TO_OFFSET(dev->last_position,
                                                  *wrt_len);
    ASSERT(RVM_OFFSET_LEQ(dev->last_position,dev->num_bytes));
    ASSERT(*wrt_len == dev->io_length);

    return 0;
    }
/* incremental write for gather-write to partitions */
static long incr_write_partition(dev,offset,start_addr,end_addr)
    device_t        *dev;               /* device descriptor */
    rvm_offset_t    *offset;            /* disk position (in/out) */
    char            *start_addr;        /* vm starting address of data */
    char            *end_addr;          /* vm ending address of data */
    {
    long            retval;             /* kernel return value */
    rvm_offset_t    tmp_offset;         /* position temp */
    rvm_length_t    length;             /* num. bytes to write (request) */
    rvm_length_t    len;                /* length corrected to sector size */
    char            *wrt_addr;          /* write address corrected to sect size */

    /* force seek to re-write partially filled sector */
    len = OFFSET_TO_SECTOR_INDEX(*offset);
    tmp_offset = CHOP_OFFSET_TO_SECTOR_SIZE(*offset);
    wrt_addr = (char *)CHOP_TO_SECTOR_SIZE(start_addr);

    /* calculate sector-sized write length */
    length = (rvm_length_t)
             RVM_SUB_LENGTH_FROM_ADDR(end_addr,start_addr);
    if (length == 0) return 0;          /* ignore null writes */
    len = ROUND_TO_SECTOR_SIZE(len+length);

    /* write */
    ASSERT(in_wrt_buf(wrt_addr,len));
    retval = write_dev(dev,&tmp_offset,wrt_addr,len,NO_SYNCH);
    if (retval < 0) return retval;
    ASSERT(len == retval);

    /* update position */
    *offset = RVM_ADD_LENGTH_TO_OFFSET(*offset,length);

    return length;
    }
/* gather write for disk partitions */
static long gather_write_partition(dev,offset,wrt_len)
    device_t        *dev;                 /* device descriptor */
    rvm_offset_t    *offset;              /* disk position */
    rvm_length_t    *wrt_len;             /* num bytes written (out) */
    {
    long            retval = 0;           /* kernel return value */
    long            iov_index = 0;        /* index of current iov entry */
    long            bytes_left;           /* num. bytes left in wrt_buf */
    io_vec_t        *iov = dev->iov;      /* i/o vector */

    rvm_bool_t      did_wrap = rvm_false; /* debug use only */
    rvm_offset_t    temp;
    rvm_length_t    len;

    ASSERT((SECTOR_INDEX(dev->ptr-dev->wrt_buf)) ==
           (OFFSET_TO_SECTOR_INDEX(*offset)));
    len = (rvm_length_t)RVM_SUB_LENGTH_FROM_ADDR(dev->ptr,
                                                 dev->buf_start);
    temp = RVM_ADD_LENGTH_TO_OFFSET(dev->sync_offset,len);
    ASSERT(RVM_OFFSET_EQL(*offset,temp)); /* must match tail */

    /* write io vector entries */
    bytes_left = (long)RVM_SUB_LENGTH_FROM_ADDR(dev->buf_end,dev->ptr);
    while (dev->iov_cnt > 0)
        {
        ASSERT(bytes_left >= 0);
        if (iov[iov_index].length <= bytes_left)
            {
            /* copy whole range into wrt_buf */
            BCOPY(iov[iov_index].vmaddr,dev->ptr,
                  iov[iov_index].length);
            bytes_left -= iov[iov_index].length;
            *wrt_len += iov[iov_index].length;
            dev->ptr = RVM_ADD_LENGTH_TO_ADDR(dev->ptr,
                                              iov[iov_index].length);
            iov_index++;                /* move to next entry */
            dev->iov_cnt--;
            }
        else
            {
            /* copy what fits and leave remainder in iov entry */
            if (bytes_left != 0)
                {
                BCOPY(iov[iov_index].vmaddr,dev->ptr,bytes_left);
                iov[iov_index].length -= bytes_left;
                *wrt_len += bytes_left;
                iov[iov_index].vmaddr =
                    RVM_ADD_LENGTH_TO_ADDR(iov[iov_index].vmaddr,
                                           bytes_left);
                }
            /* write what's in wrt_buf & re-init buffer */
            if (dev->buf_start != dev->buf_end)
                {
                retval=incr_write_partition(dev,&dev->sync_offset,
                                            dev->buf_start,dev->buf_end);
                if (retval < 0) return retval;
                }
            did_wrap = rvm_true;
            dev->ptr = dev->buf_start = dev->wrt_buf;
            bytes_left = dev->wrt_buf_len;
            }
        }

    ASSERT((retval >= 0) ? (*wrt_len == dev->io_length) : 1);
    return retval;
    }
/* gather write to device: accepts vector of any length
   pointed to by device descriptor */
long gather_write_dev(dev,offset)
    device_t        *dev;               /* device descriptor */
    rvm_offset_t    *offset;            /* device offset */
    {
    long            retval;             /* kernel return value */
    rvm_length_t    wrt_len = 0;        /* #bytes actually written */

    ASSERT(RVM_OFFSET_GEQ(*offset,default_log->status.log_start));
    ASSERT(RVM_OFFSET_LSS(*offset,dev->num_bytes));
    ASSERT(RVM_OFFSET_LEQ(dev->last_position,dev->num_bytes));

    errno = 0;

    /* select gather-write mechanism for partitions or files */
    if (dev->raw_io)
        retval = gather_write_partition(dev,offset,&wrt_len);
    else
        retval = gather_write_file(dev,offset,&wrt_len);

    if (retval < 0) return retval;      /* error */

    return wrt_len;
    }
/* sync file */
long sync_dev(dev)
    device_t        *dev;               /* device descriptor */
    {
    long            retval;

    ASSERT(dev->handle != 0);
    ASSERT(((dev == &default_log->dev) && (!rvm_utlsw)) ?
           (!LOCK_FREE(default_log->dev_lock)) : 1);
    errno = 0;

    /* use kernel call for file sync */
    if (!dev->raw_io)
	{
	retval = fsync((int)dev->handle);
	if (retval<0)
	    {
	    rvm_errdev = dev;
	    rvm_ioerrno = errno;
	    }
        return retval;
	}

    /* raw i/o flushes buffer */
    retval = incr_write_partition(dev,&dev->sync_offset,
                                  dev->buf_start,dev->ptr);
    if (retval >= 0)
        dev->buf_start = dev->ptr;

    return retval;
    }



