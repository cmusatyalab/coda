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



#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include "coda_string.h"

#if defined(__APPLE__) && defined(__MACH__)
#include <mach/mach.h>
#endif

#ifdef __cplusplus
}
#endif __cplusplus

#include <parser.h>
#include "norton.h"

command_t del_commands[] = {
//    { "directory",	notyet,		0,	""},
//    { "inode",		notyet,		0,	""},
    { "name",		delete_name,		0,	""},
//    { "vnode",		notyet,		0,	""},
    { "volume",		sh_delete_volume,	0,	""},
    { 0, 0, 0, ""}
};

command_t create_commands[] = {
    { "name",		sh_create_name,		0,	""},
    { 0, 0, 0, ""}
};

command_t salvage_commands[] = {
    { "all",  		notyet,		0,	""},
    { "directory",	notyet,		0,	""},
    { "inode",		notyet,		0,	""},
    { "resolution",	notyet,		0,	""},
    { "vnode",		notyet,		0,	""},
    { "volume",		notyet,		0,	""},
    { 0, 0, 0, ""}
};

command_t show_vol_cmds[] = {
    { "details",	show_volume_details,	0,	""},
    { 0, 0, 0, ""},
};


command_t show_cmds[] = {
    { "debug", 		show_debug,	0,	""},
    { "directory",	show_dir,	0,	""},
    { "free",		show_free,	0,	""},
    { "heap",		show_heap,	0,	""},
    { "index",		show_index,	0,	""},
//    { "inode",		notyet,		0,	""},
    { "vnode",		show_vnode,	0,	""},
    { "volume",		show_volume,	show_vol_cmds,	""},
    { 0, 0, 0, ""}
};

command_t list_cmds[] = {
    { "volumes",	list_vols,	0,	""},
    { 0, 0, 0, ""}
};

command_t set_cmds[] = {
    { "linkcount",	set_linkcount,	0,	"Set linkcount in vnode (args: vol vnode uniq count)"},
    { "debug",	set_debug,	0,	"Set debug level (args: level)"},
    { 0, 0, 0, ""}
};



command_t commands[] = {
    { "?",		Parser_qhelp, 	0,		  ""},
    { "delete",	 	0,		del_commands,	  ""},
    { "create",         0,              create_commands,  ""},
    { "examine",	examine,	0,		  ""},
    { "exit",		Parser_exit,	0,		  ""},
    { "help",		Parser_qhelp,	0,		  ""},
    { "list",		list_vols,	list_cmds,	  ""},
    { "quit",		Parser_exit,	0,		  ""},
//  { "salvage",	0,		salvage_commands, ""},
    { "show",		0,		show_cmds,	  ""},
    { "set",		0,		set_cmds,	  ""},
//  { "truncate",	notyet,		0,		  ""},
    { "x",		examine, 	0,		  ""},
    { 0, 0, 0, ""}
};

void InitParsing() {
    Parser_init("norton> ", &commands[0]);
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


#ifdef __MACH__
#ifdef __APPLE__
static long address_ok(vm_address_t addr, vm_size_t size, vm_prot_t perm)
{
    kern_return_t  rc;
    struct vm_region_basic_info info;
    mach_msg_type_number_t infoCnt;
    vm_address_t current, end;
    mach_port_t object_name;

    infoCnt = VM_REGION_BASIC_INFO_COUNT;

    current = addr;
    end = addr + size;
    while (current < end) {
        rc = vm_region(mach_task_self(), &current, &size, VM_REGION_BASIC_INFO, (vm_region_info_t)&info, &infoCnt, &object_name);
        if (rc != KERN_SUCCESS)
            return 0;

        if ((info.protection & perm) != perm)
            return 0;

        current += size;
    }
    return 1;
}
#else
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
#endif /* !__APPLE__ */
#endif /* __MACH__ */

#if    defined	(__linux__) || defined(__CYGWIN32__) || defined(sun)
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

#if defined(BSD4_4) && !defined(__APPLE__)
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
	(Parser_int(argv[1], (int *)&base) != 1) ||
	(Parser_int(argv[2], &len) != 1)) {
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
	(Parser_int(argv[2], &debug_level) != 1)) {
	fprintf(stderr, "Usage: set debug <debug_level>\n");
	return;
    }

    norton_debug = debug_level;
    printf("Debug level set to: %d\n", norton_debug);
}

void show_debug(int argc, char *argv[]) {
    printf("Debug level: %d\n", norton_debug);
}


