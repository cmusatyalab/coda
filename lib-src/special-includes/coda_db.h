/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef CODA_DB_H
#define CODA_DB_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_NDBM
#include <ndbm.h>
#elif defined(HAVE_DB1_DB_H)
#include <db1/db.h>
#elif defined(HAVE_DB_185_H)
#include <db_185.h>
#else
#include <db.h>
#endif

#ifdef HAVE_NDBM
#define		DB_BTREE				0
#define		db_type					DBM
#define		db_data					datum
#define		db_dataptr				dptr
#define		db_datasize				dsize
#define		db_put(db,key,content,flags)		dbm_store(db,*(key),*(content),flags)
#define		db_get(db,key,content,flags,ret)	do { *(content) = dbm_fetch(db,*(key)); \
							     ret = (content)->dptr == NULL ? -1 : 0; } \
								while (0);
#define		db_open(db,open_flags,file_flags,type,info)	dbm_open(db,open_flags,file_flags)
#define		db_close(db)				dbm_close(db)
#define		db_first(db,key,content,ret)		do { *(key) = dbm_firstkey(db); \
							     if ((key)->dptr != NULL) \
							  	*(content) = dbm_fetch(db,*(key)); \
							     ret = (key)->dptr == NULL ? -1 : 0; } \
								while (0);
#define		db_next(db,key,content,ret)			do { *(key) = dbm_nextkey(db); \
							     if ((key)->dptr != NULL) \
							  	*(content) = dbm_fetch(db,*(key)); \
							     ret = (key)->dptr == NULL ? -1 : 0; } \
							     	while (0);
#define		db_del(db,key,flags)			dbm_delete(db,*(key))
#define		RET_SUCCESS				0
#define		DB_PUT_FLAG				DBM_REPLACE
#else
#define		db_type					DB
#define		db_data					DBT
#define		db_dataptr				data
#define		db_datasize				size
#define		db_put(db,key,content,flags)		db->put(db,key,content,flags)
#define		db_get(db,key,content,flags,ret)	(ret = db->get(db,key,content,flags))
#define		db_open(db,open_flags,file_flags,type,info)	dbopen(db,open_flags,file_flags, type, info)
#define		db_close(db)				db->close(db)
#define		db_first(db,key,content,ret)		(ret = db->seq(db,key,content, R_FIRST))
#define		db_next(db,key,content,ret)		(ret = db->seq(db,key,content, R_NEXT))
#define		db_del(db,key,flags)			db->del(db,key,flags)
#define		DB_PUT_FLAG				0
#endif

#endif
