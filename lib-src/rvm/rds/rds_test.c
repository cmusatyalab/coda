/* BLURB lgpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the  terms of the  GNU  Library General Public Licence  Version 2,  as
shown in the file LICENSE. The technical and financial contributors to
Coda are listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/



/*
 * this file was written to test the recoverable heap stuff for rvm.
 */

#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include "rds_private.h"

extern char *sys_errlist[];
extern int sys_nerr;

void PollAndYield()
{
}

main(argc, argv)
     int  argc;
     char *argv[];
{
    rvm_options_t       *options;       /* options descriptor ptr */
    rvm_return_t	ret;
    int err;
    char string[80], DataDev[80], *sptr;
    unsigned long vmaddr, length, i;
    rvm_region_def_t regions[20];
    rvm_tid_t *tid = rvm_malloc_tid();
    rvm_offset_t DataLen;
    struct stat sbuf;
    
    if (argc < 3) {
	printf("Usage: %s log-device data-device\n",argv[0]);
	exit(-1);
    }
    
    options = rvm_malloc_options();
    options->log_dev = argv[1];
    strcpy(DataDev, argv[2]);
    
    if (stat(DataDev, &sbuf) < 0) {
	printf("%s\n", errno < sys_nerr? sys_errlist[errno]: "Cannot stat");
	exit(-1);
    }

    switch (sbuf.st_mode & S_IFMT) {
      case S_IFSOCK:
      case S_IFDIR:
      case S_IFLNK:
      case S_IFBLK: 
	printf("Illegal file type!\n");
	exit(-1);

      case S_IFCHR:
	printf("Enter the length of the device %s: ", DataDev);
	gets(string);
	
	sptr = string;
	if ((*sptr == '0') && (*(++sptr) == 'x'))
	    sscanf(string, "0x%X", &length);
	else
	    length = atoi(string);

	DataLen = RVM_MK_OFFSET(0, length);
	break;
	
      default:		/* Normal files. */
	DataLen = RVM_MK_OFFSET(0, sbuf.st_size);
    }
	
    for (;;) {
	printf("> ");
	gets(string);
	
	switch (*string) {
	  case 'i' :
	      ret = RVM_INIT(options);
	      if  (ret != RVM_SUCCESS) 
		  printf("?rvm_initialize failed, code: %s\n",rvm_return(ret));
	      else
		  printf("rvm_initialize succeeded.\n");
	      break;

	  case 'C' :
	      for (i = 0; i < 20; i++) {
		  printf("region %d:\n",i);
		  printf("    vmaddr> ");	gets(string);
		  if (*string == 'q') break;
		  sscanf(string, "%x", &regions[i].vmaddr);
		  printf("    length> ");	gets(string);
		  sscanf(string, "%x", &regions[i].length);
	      }

	      ret = rvm_create_segment(DataDev, DataLen, i, regions, options);
	      if (ret != RVM_SUCCESS)
		  printf("ERROR:rvm_create_segment failed, code: %s\n",
			 rvm_return(ret));
	      else
		  printf("rvm_create_segment succeeded.\n");
	      break;

	  case 't' :
	      ret = rvm_terminate();
	      if (ret != RVM_SUCCESS)
		  printf("\n? Error in rvm_terminate, ret = %s\n",
			 rvm_return(ret));
	      else
		  printf("rvm_terminate succeeded.\n");
	      break;
	      

	  case 'l' : {
	      unsigned long nregions;
	      rvm_region_def_t *regions;
	      printf("%x\n",sbrk(0));
	      ret = rvm_load_segment(DataDev,DataLen,options,&nregions, &regions);
	      if (ret != RVM_SUCCESS) 
		  printf("ERROR: rvm_load_seg, code: %s\n",rvm_return(ret));
	      else
		  printf("rvm_load_segment succeeded.\n");
	      free(regions);
	      break;
	  }
	      
	  case 'q' :
		exit(0);

	  case 'Q' : { /* Query RVM */
	      rvm_options_t *curopts;
	      int i;
	      
	      BZERO(&regions[0], sizeof(rvm_region_def_t));
	      ret = rvm_query(&curopts, &regions[0]);

	      printf("Uncommitted transactions: %d\n", curopts->n_uncommit);

	      for (i = 0; i < curopts->n_uncommit; i++) {
		  rvm_abort_transaction(&(curopts->tid_array[i]));
		  if (ret != RVM_SUCCESS) 
		      printf("ERROR: abort failed, code: %s\n", rvm_return(ret));
	      }

	      BZERO(&regions[0], sizeof(rvm_region_def_t));
	      ret = rvm_query(&curopts, &regions[0]);

	      printf("Uncommitted transactions: %d\n", curopts->n_uncommit);

	      rvm_free_options(curopts);
	      break;
	  }

	  case 'b' : {
	      rvm_tid_t *ttid = rvm_malloc_tid();

	      ret = rvm_begin_transaction(ttid, no_restore);
	      if (ret != RVM_SUCCESS) {
		  printf("ERROR: begin_trans failed, code: %s\n",rvm_return(ret));
		  break;
	      }

	      printf("Transaction started, tid = 0x%x\n", ttid);
	      break;
	  }

	  case 'a' : {
	      rvm_tid_t *ttid = rvm_malloc_tid();
	      
	      printf("address of transaction to abort: 0x"); gets(string);
	      sscanf(string, "%x", (int *)&ttid);

	      ret = rvm_abort_transaction(ttid);
	      if (ret != RVM_SUCCESS) 
		  printf("ERROR: abort failed, code: %s\n", rvm_return(ret));

	      break;
	  }

	  case 'e' : {
	      rvm_tid_t *ttid = rvm_malloc_tid();
	      
	      printf("address of transaction to end: 0x"); gets(string);
	      sscanf(string, "%x", (int *)&ttid);

	      ret = rvm_end_transaction(ttid, flush);
	      if (ret != RVM_SUCCESS) 
		  printf("ERROR: abort failed, code: %s\n", rvm_return(ret));

	      break;
	  }
	      
	  case 'w' : {
	      int len;
	      int *addr;

	      printf("address to write to: "); gets(string);
	      sscanf(string, "%x", (int *)&addr);
	      printf("Number of words to write: "); gets(string);
	      sscanf(string, "%d", &len);

	      ret = rvm_begin_transaction(tid, no_restore);
	      if (ret != RVM_SUCCESS) {
		  printf("ERROR: begin_trans failed, code: %s\n",rvm_return(ret));
		  break;
	      }
	      
	      ret = rvm_set_range(tid, (char *)addr, len * sizeof(int));
	      if (ret != RVM_SUCCESS) {
		  printf("ERROR: set_range, code: %s, abort: %s\n",
			 rvm_return(ret),
			 rvm_return(rvm_abort_transaction(tid)));
	      }
	      
	      for (i = 0; i < len; i++)
		  addr[i] = i;

	      ret = rvm_end_transaction(tid, flush);
	      if (ret != RVM_SUCCESS)
		  printf("ERROR: end_trans, code: %s\n",rvm_return(ret));

	      break;
	  }

	  case 'r' : {
	      int len;
	      int *addr;
	      
	      printf("address to read from: "); gets(string);
	      sscanf(string, "%x", (int *)&addr);
	      printf("Number of words to read: "); gets(string);
	      sscanf(string, "%d", &len);

	      for (i = 0; i < len; i++)
		  printf("%d\n", addr[i]);
	      break;
	  }

	  case 'z' : { /* Create a heap */
	      char *static_addr;
	      int slen, hlen, nlists, chunksize;
	      printf("Create a dynamic heap.\n");
	      printf("starting address of rvm: "); gets(string);
	      sscanf(string, "%x", (int *)&static_addr);
	      printf("heap len, static len: "); gets(string);
	      sscanf(string, "%x, %x", &hlen, &slen);
	      printf("nlists: "); gets(string); nlists = atoi(string);
	      printf("chunksize: "); gets(string); chunksize = atoi(string);

	      rds_zap_heap(DataDev, DataLen, static_addr, slen, hlen, nlists, chunksize, &err);
	      if (err == SUCCESS)
		  printf("rds_zap_heap completed successfully.\n");
	      else if (err > SUCCESS)
		  printf("ERROR: rds_zap_heap %s.\n",
			 rvm_return((rvm_return_t) err));
	      else
		  printf("ERROR: rds_zap_heap, code: %d\n", err);

	      break;
	  }
 
	  case 'm' : {
	      int s, n, i;
	      char *temp;

	      printf("Size of object to allocate: "); gets(string);
	      s = atoi(string);
	      printf("Number of objects to allocate: "); gets(string);
	      n = atoi(string);

	      for (i = 0; i < n; i++) {
		  temp = rds_malloc(s, 0, &err);
		  printf("%d: rds_malloc = %d, &object = 0x%x\n", i, err, temp);
	      }
	      break;
	  }

	  case 'f' : {
	      char *temp;
	      
	      printf("object to free: 0x"); gets(string);
	      sscanf(string, "%x", (int *)&temp);
	      rds_free(temp, 0, &err);
	      if (err != SUCCESS) {
		  if (err > SUCCESS)
		      printf("rds_free = %s\n", rvm_return((rvm_return_t)err));
		  else
		      printf("rds_free = %d\n", err);
	      }
	      break;
	  }

	  case 'x' : { /* init_heap */
	      rvm_tid_t *tid = rvm_malloc_tid();
	      unsigned long hlen, slen, chunksize, nlists;
	      char *startAddr;
	      
	      /* Start a transaction to initialize the heap */
	      printf("Initialize heap.\n");
	      printf("starting address of rvm: "); gets(string);
	      sscanf(string, "%x", (int *)&startAddr);
	      printf("heap len, static len: "); gets(string);
	      sscanf(string, "%x, %x", &hlen, &slen);
	      printf("nlists: "); gets(string); nlists = atoi(string);
	      printf("chunksize: "); gets(string); chunksize = atoi(string);

	      ret = rvm_begin_transaction(tid, no_restore);
	      if (ret != RVM_SUCCESS)
		  printf("begin_trans code %s\n",rvm_return(ret));
	      
	      rds_init_heap(startAddr, hlen, chunksize, nlists, tid, &err);
	      if (err > SUCCESS) /* RVM error */ 
		  printf("init_heap code %s\n",rvm_return((rvm_return_t)err));

	      if (err < SUCCESS)
		  printf("init_heap code %d\n",err);
	      
	      ret = rvm_end_transaction(tid, flush);
	      if (ret != RVM_SUCCESS) 
		  printf("end_trans code %s\n",rvm_return(ret));

	      rvm_free_tid(tid);
	      break;
	  }

	  case 's' : { /* Start Heap */
	      char *addr;

	      rds_load_heap(DataDev, DataLen, &addr, &err);
	      if (err == SUCCESS)
		  printf("rds_load_heap successful\n");
	      else if (err > SUCCESS)
		  printf("rds_load_heap = %s\n", rvm_return((rvm_return_t)err));
	      else
		  printf("rds_load_heap = %d\n", err);
	      break;
	  }

	  case 'c' :
	      ret = rvm_begin_transaction(tid, no_restore);
	      if (ret != RVM_SUCCESS)
		  printf("begin_trans code %s\n",rvm_return(ret));

	      ret = rvm_set_range(tid, &RDS_STATS, sizeof(rds_stats_t));
	      if (ret != RVM_SUCCESS) {
		  printf("Couldn't setrange for stats %s.", rvm_return(ret));
		  break;
	      }
	      
	      coalesce(tid, &err);
	      if (err == SUCCESS)
		  printf("rds_coalesce successful\n");
	      else if (err > SUCCESS)
		  printf("rds_coalesce = %s\n", rvm_return((rvm_return_t)err));
	      else
		  printf("rds_coalesce = %d\n", err);

	      ret = rvm_end_transaction(tid, flush);
	      if (ret != RVM_SUCCESS) 
		  printf("end_trans code %s\n",rvm_return(ret));

	      break;
	      
          case 'p' :
	      print_heap();
	      break;

	  case '+' :
	      rds_print_stats();
	      break;

	  case '-' :
	      rds_clear_stats(&err);
	      printf("Cleared stats %d\n", err);
	      break;
	      
	  case '*' : {
	      rds_stats_t stats;
	      
	      rds_get_stats(&stats);
	      break;
	  }
	      
	  case '?' :
	      printf("One letter commands are:\n");
	      printf("p	\t Print out the heap free lists.\n");
	      printf("c	\t Coalesce the heap.\n");
	      printf("C	\t Create a new segment header.\n");
	      printf("i	\t Initialize RVM.\n");
	      printf("f	\t Free a memory object.\n");
	      printf("h	\t Create a heap in a segment.\n");
	      printf("l	\t Load in a segment.\n");
	      printf("m	\t Allocate a memory object.\n");
	      printf("q	\t Quit this program.\n");
	      printf("r	\t Read some number of values from a segment.\n");
	      printf("s	\t Start the heap - eg load in the sement.\n");
	      printf("t	\t Terminate RVM.\n");
	      printf("w	\t Write some number of values to a segment.\n");
	      printf("x	\t Initialize the heap (not the segment!).\n");
	      printf("+ \t Print the heap usage statistics.\n");
	      printf("?	\t This Help Message.\n");
	      break;
	  }

	/* Need to flush any changes that were made. */
	if (*string != 't') { /* Only if terminate hasn't been called. */
	    ret = rvm_flush();
    	    if (ret != RVM_SUCCESS)
		printf("Flush failed %s\n", rvm_return(ret));
	}
    }
}
