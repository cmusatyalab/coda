#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "$Header: /coda/coda.cs.cmu.edu/project/coda/cvs/coda/coda-src/norton/commands.cc,v 4.3 1997/10/15 15:53:01 braam Exp $";
#endif /*_BLURB_*/



#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#ifdef	__MACH__
#include <mach.h>
#endif
#ifdef __cplusplus
}
#endif __cplusplus

#include "parser.h"
#include "norton.h"

command_t del_commands[] = {
//    { "directory",	notyet,		0},
//    { "inode",		notyet,		0},
    { "name",		delete_name,		0},
//    { "vnode",		notyet,		0},
    { "volume",		sh_delete_volume,	0},
    { 0, 0, 0}
};

command_t create_commands[] = {
    { "name",		sh_create_name,		0},
    { 0, 0, 0}
};

command_t salvage_commands[] = {
    { "all",  		notyet,		0},
    { "directory",	notyet,		0},
    { "inode",		notyet,		0},
    { "resolution",	notyet,		0},
    { "vnode",		notyet,		0},
    { "volume",		notyet,		0},
    { 0, 0, 0}
};

command_t show_vol_cmds[] = {
    { "details",	show_volume_details,	0},
    { 0, 0, 0},
};


command_t show_cmds[] = {
    { "debug", 		show_debug,	0},
    { "directory",	show_dir,	0},
    { "free",		show_free,	0},
    { "heap",		show_heap,	0},
    { "index",		show_index,	0},
//    { "inode",		notyet,		0},
    { "vnode",		show_vnode,	0},
    { "volume",		show_volume,	show_vol_cmds},
    { 0, 0, 0}
};

command_t list_cmds[] = {
    { "volumes",	list_vols,	0},
    { 0, 0, 0}
};

command_t set_cmds[] = {
    { "debug",	set_debug,	0},
    { 0, 0, 0}
};



command_t commands[] = {
    { "?",		quick_help, 	0},
    { "delete",	 	0,		del_commands},
    { "create",         0,              create_commands},
    { "examine",	examine,	0},
    { "exit",		exit_parser,	0},
    { "help",		quick_help,	0},
    { "list",		list_vols,	list_cmds},
    { "quit",		exit_parser,	0},
//    { "salvage",	0,		salvage_commands},
    { "show",		0,		show_cmds},
    { "set",		0,		set_cmds},
//    { "truncate",	notyet,		0},
    { "x",		examine, 	0},
    { 0, 0, 0}
};

void InitParsing() {
    init_parser("norton> ", &commands[0]);
}

void notyet(int argc, char *argv[]) {
    char buf[80];
    int  i;

    buf[0] = '\0';
    for (i = 0; i < argc; i++) {
	strcat(buf, argv[i]);
	strcat(buf, " ");
    }
    fprintf(stderr, "'%s' has not been implemented yet.\n", buf);
}


#ifdef	__MACH__
static
long address_ok(vm_address_t addr, vm_size_t sz, vm_prot_t perm)
{
    vm_address_t    address = addr;
    vm_size_t       size;
    vm_prot_t       protection;
    vm_prot_t       max_protection;
    vm_inherit_t    inheritance;
    boolean_t       shared;
    port_t          object_name;
    vm_offset_t     offset;

    while(vm_region(task_self(), &address, &size,
		    &protection, &max_protection,
		    &inheritance, &shared,
		    &object_name, &offset) == KERN_SUCCESS) {
	if (address > addr) return(0);
	if ((protection & perm) != perm) return(0);
	if (address + size >= addr + sz) return(1);
	sz = (addr + sz) - (address + size);
	addr = address += size;
	if (size == 0) break;
    }
    return(1);
}
#endif

#ifdef	__linux__
#include <sys/mman.h>
#define vm_address_t caddr_t
#define vm_size_t    size_t
#define vm_prot_t    int
#define VM_PROT_READ PROT_READ
static
long address_ok(vm_address_t addr, vm_size_t sz, vm_prot_t perm)
{
  return mprotect(addr, sz, perm);
}
#endif

#ifdef __BSD44__
#include <sys/mman.h>
#define vm_address_t caddr_t
#define vm_size_t    size_t
#define vm_prot_t    int
#define VM_PROT_READ PROT_READ
static
long address_ok(vm_address_t addr, vm_size_t sz, vm_prot_t perm)
{
  fprintf(stderr, "Someone needs to write code for address_ok.\n");
  return 1;
}
#endif

#define BYTES_PER_LINE	16
void examine(int argc, char *argv[]) {
    int  *base,
	 *addr;
    char *buf;
    int  len,
	i;
    
    if ((argc != 3) ||
	(parse_int(argv[1], (int *)&base) != 1) ||
	(parse_int(argv[2], &len) != 1)) {
	fprintf(stderr, "Usage: examine <addr> <len>\n");
	return;
    }

    for (addr = base; addr - base <len; addr += BYTES_PER_LINE/sizeof(int)) {
	
	if (!address_ok((vm_address_t)addr,
			(vm_size_t)BYTES_PER_LINE/(int)sizeof(int),
			VM_PROT_READ)) { 
	    printf("ERROR reading address: 0x%08x\n", addr);
	    break;
	}
    
	printf("0x%08x: ", addr);
	for (i = 0; i < BYTES_PER_LINE/sizeof(int); i++) {
	    printf("  0x%08x", *(addr + i));
	}
	
	printf("  |");
	for (buf = (char *)addr; buf - (char *)addr < BYTES_PER_LINE; buf++) {
	    if ((*buf >= (char )32 && *buf <= (char)126) || *buf >= (char)161)  
		printf("%c", *buf);
	    else
		printf(".");
	}
	printf("|\n");
    }
}


void set_debug(int argc, char *argv[]) {
    int debug_level;
    
    if ((argc != 3) ||
	(parse_int(argv[2], &debug_level) != 1)) {
	fprintf(stderr, "Usage: set debug <debug_level>\n");
	return;
    }

    norton_debug = debug_level;
    printf("Debug level set to: %d\n", norton_debug);
}

void show_debug(int argc, char *argv[]) {
    printf("Debug level: %d\n", norton_debug);
}


