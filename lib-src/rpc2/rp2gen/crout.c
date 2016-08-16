/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-2016 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights

#*/

/*
                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.   This  code is provided "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to  modify,  distribute and sublicense this code,  which is
based on Version 2  of  AFS  and  does  not  contain  the features and
enhancements that are part of  Version 3 of  AFS.  Version 3 of AFS is
commercially   available   and  supported  by   Transarc  Corporation,
Pittsburgh, PA.

*/


/*******************************************************************\
* 								    *
*	Routines for handling C.				    *
*								    *
\*******************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/param.h>
#include <string.h>
#include <rpc2/pack_helper.h>

#include "rp2.h"


#define _PAD(n)((((n)-1) | 3) + 1)
#define BUFFEROVERFLOW  "\tgoto bufferoverflow;\n"
#define BUFFEROVERFLOW_END  "\nbufferoverflow:\n    fprintf(stderr,\"%%s:%%d Buffer overflow in (un)marshalling !\\n\",__FILE__,__LINE__);\n"

static int buffer_checked = 0;

static void dump_procs(PROC *head, FILE *where);
static void print_type(RPC2_TYPE *t, FILE *where, char *name);
static void print_var(VAR *v, FILE *where);
static void locals(FILE *where, const char *who);
static void common(FILE *where);
static void client_procs(PROC *head, FILE *where);
static void one_client_proc(PROC *proc, FILE *where);
static void spit_parm(VAR *parm, WHO who, FILE *where, rp2_bool header);
static void for_limit(VAR *parm, WHO who, FILE *where);
static void spit_body(PROC *proc, rp2_bool in_parms, rp2_bool out_parms, FILE *where);
static char *field_name(VAR *parm, char *prefix);
static char *field_name2(VAR *parm, char *prefix, char *suffix);
static void array_print_size(WHO who, VAR *parm, char *prefix, FILE *where);
static void print_size(WHO who, VAR *parm, char *prefix, FILE *where);
static void inc(char *what, char *by, FILE *where);
static void inc4(char *what, FILE *where);
static void set_timeout(PROC *proc, FILE *where);
static void pack(WHO who, VAR *parm, char *prefix, FILE *where);
static void unpack(WHO who, VAR *parm, char *prefix, FILE *where);
static void server_procs(PROC *head, FILE *where);
static void check_new_connection(PROC *proc);
static void one_server_proc(PROC *proc, FILE *where);
static void free_boundedbs(VAR *parm, FILE *where);
static void alloc_dynamicarray(VAR *parm, WHO who, FILE *where);
static void free_dynamicarray(VAR *parm, FILE *where);
static void pass_parm(VAR *parm, FILE *where);
static void execute(PROC *head, FILE *where);
static void multi_procs(PROC *head, FILE *where);
static void pr_size(VAR *parm, FILE *where, rp2_bool TOP, int32_t proc, int32_t arg);
static void do_struct(VAR **fields, int32_t proc, int32_t arg, int32_t level, int32_t cur_struct, FILE *where);
static void macro_define(FILE *where);
static void version_check(FILE *where);
static void declare_CallCount(PROC *head, FILE *where);
static void declare_MultiCall(PROC *head, FILE *where);
static void declare_LogFunction(PROC *head, FILE *where);
/* the stubpredefined structs have been moved to rpc2/rpc2.h */
//static void print_stubpredefined(FILE *where);
void print_struct_func(RPC2_TYPE *t, FILE *where, FILE *hfile, char *name);
void print_pack_var(char* prefix, VAR* var, FILE *where);
void print_unpack_var(char* prefix, VAR* var, FILE *where);

extern char *concat(), *concat3elem(), *server_prefix, *client_prefix;;
extern rp2_bool testing;
extern rp2_bool cplusplus;
extern rp2_bool neterrors;  /* exchange OS independent errors */
extern struct subsystem subsystem;
extern unsigned versionnumber;	/* used to check version */
extern ENTRY *find();

enum {INCLIENTS, INSERVERS, NEITHER} WhatAmIDoing;



/* Arrays of string values for printing argument modes and types */
char *MultiModes[] = {  "NO_MODE",
			"IN_MODE",
			"OUT_MODE",
			"IN_OUT_MODE",
			"C_END"
};

char *MultiTypes[] = {	"RPC2_INTEGER_TAG",
			"RPC2_UNSIGNED_TAG",
			"RPC2_BYTE_TAG",
			"RPC2_STRING_TAG",
			"RPC2_COUNTEDBS_TAG",
			"RPC2_BOUNDEDBS_TAG",
			"RPC2_BULKDESCRIPTOR_TAG",
			"RPC2_ENCRYPTIONKEY_TAG",
			"RPC2_STRUCT_TAG",
			"RPC2_ENUM_TAG",
			"RPC2_DOUBLE_TAG"
};

void cinclude(char *filename, WHO who, FILE *where)
{
    char ifdefname[MAXPATHLEN+1], spitname[MAXPATHLEN+1];
    char *p, *s;


    if (filename == NULL)
	return;
    s = ifdefname;
    *s++ = '_';
    for (p = filename; *p != 0; p++)
	if (isalnum(*p))
	    *s++ = *p;
	else
	    *s++ = '_';
    *s++ = '_';
    *s++ = 0;
    fprintf(where, "#ifndef %s\n", ifdefname);
    fprintf(where, "#define %s\n", ifdefname);

    /* If input file has extension ".rpc2", change it to ".h"
       The ".h" file must be generated by running rp2gen separately
       on the ".rpc2" file.  This strategy allows the type definitions
       in the ".rpc2" file to be used in many subsystems. */
    strcpy(spitname, filename);
    s = strrchr(spitname, '.');
    if (s && strcmp(s, ".rpc2") == 0) strcpy(s, ".h");
    fprintf(where, "#include \"%s\"\n", spitname);
    fprintf(where, "#endif /* %s */\n", ifdefname);
}

void cdefine(char *id, char *value, WHO who, FILE *where)
{
    fprintf(where, "#define %s\t%s\n", id, value);
}

void ctype(ENTRY *e, WHO who, FILE *where)
{
    fprintf(where, "\ntypedef ");
    if (e->defined != NIL)
	fprintf(where, "%s", e->defined->name);
    else
	print_type(e->type, where, e->name);  /* used only for type structs */
    fprintf(where, " %s", e->name);
    if (e->bound != NIL) fprintf(where, "[%s]", e->bound);
    fprintf(where, ";\n");
}

static rp2_bool legal_struct_fields[] = {

	/* RPC2_Integer */		RP2_TRUE,
	/* RPC2_Unsigned */	RP2_TRUE,
	/* RPC2_Byte */		RP2_TRUE,
	/* RPC2_String */		RP2_TRUE,	/* Untested */
	/* RPC2_CountedBS */	RP2_TRUE, /* Untested */
	/* RPC2_BoundedBS */	RP2_TRUE,
	/* RPC2_BulkDescriptor */	RP2_FALSE,  /* Untested */
	/* RPC2_EncryptionKey */	RP2_TRUE,   /* Untested */
	/* RPC2_Struct */		RP2_TRUE,
	/* RPC2_Enum */		RP2_TRUE,
	/* RPC2_Double */		RP2_TRUE
};
void print_struct_func(RPC2_TYPE *t, FILE *where, FILE *hfile, char *name) {
    if (t->tag != RPC2_STRUCT_TAG)
        return;
    fprintf(where, "\nint pack_struct_%s (BUFFER *buf, %s *ptr)\n{\n", name, name);
    fprintf(hfile, "int pack_struct_%s (BUFFER *buf, %s *ptr);\n", name, name);

    VAR **v;
    for (v=t->fields.struct_fields; *v!=NIL; v++) {
        if (!legal_struct_fields[(int32_t) (*v)->type->type->tag]) {
            printf("RP2GEN: illegal type for RPC2_Struct field: %s\n", (*v)->name);
            exit(1);
        }
        print_pack_var("ptr->", *v, where);
    }
    fprintf(where, "    return 0;\n}\n");
    fprintf(where, "\nint unpack_struct_%s (BUFFER *buf, %s *ptr)\n{\n", name, name);
    fprintf(hfile, "int unpack_struct_%s (BUFFER *buf, %s *ptr);\n", name, name);

    for (v=t->fields.struct_fields; *v!=NIL; v++) {
        if (!legal_struct_fields[(int32_t) (*v)->type->type->tag]) {
            printf("RP2GEN: illegal type for RPC2_Struct field: %s\n", (*v)->name);
            exit(1);
        }
        print_unpack_var("ptr->", *v, where);
    }
    fprintf(where, "    return 0;\n}\n");
}
void print_unpack_var(char* prefix, VAR* var, FILE *where) {
    extern char* concat();
    char* name, *suffix;
    MODE mode;
    char _iterate[] = "_iterate";

    name = concat(prefix, var->name);
    suffix = concat3elem("[", _iterate, "]");
    mode = var->mode;
    switch(var->type->type->tag) {
            case RPC2_INTEGER_TAG:
                    fprintf(where, "    if (unpack_int(buf, &(%s)))\n", name);
                    fprintf(where, "        return -1;\n");
                    break;
            case RPC2_UNSIGNED_TAG:
                    fprintf(where, "    if (unpack_unsigned(buf, &(%s)))\n", name);
                    fprintf(where, "        return -1;\n");
                    break;
            case RPC2_BYTE_TAG:
                    if (var->type->bound != NIL) {
                        fprintf(where, "    if (unpack_bound_bytes(buf, (unsigned char *)%s, %s))\n", name, var->type->bound);
                        fprintf(where, "        return -1;\n");
                    } else {
                        fprintf(where, "    if (unpack_unbound_bytes(buf, (unsigned char *)(&%s)))\n",name);
                        /* in struct, the following condition is not possible */
                        //if (who == RP2_CLIENT && mode == IN_OUT_MODE)
                        //    fputs("*", where);
                        fprintf(where, "        return -1;\n");
                    }
                    break;
            case RPC2_STRING_TAG:
                    fprintf(where, "    if (unpack_string(buf, (unsigned char **)&(%s)))\n", name);
                    fprintf(where, "        return -1;\n");
                    break;
            case RPC2_COUNTEDBS_TAG:
                    fprintf(where, "    if (unpack_countedbs(buf, &(%s)))\n", name);
                    fprintf(where, "        return -1;\n");
                    break;
            case RPC2_BOUNDEDBS_TAG:
                    fprintf(where, "    if (unpack_boundedbs(buf, %d, &(%s)))\n", mode, name);
                    fprintf(where, "        return -1;\n");
                    break;
            case RPC2_BULKDESCRIPTOR_TAG:
                    break;
            case RPC2_ENCRYPTIONKEY_TAG:
                    fprintf(where, "    if (unpack_encryptionKey(buf, (char *)%s))\n", name);
                    fprintf(where, "        return -1;\n");
                    break;
            case RPC2_DOUBLE_TAG:
                    fprintf(where, "    if (unpack_double(buf, &(%s)))\n", name);
                    /* in struct, the following condition is not possible */
                    //if (who == RP2_CLIENT && mode == IN_OUT_MODE)
                    //    fputs("*", where);
                    fprintf(where, "        return -1;\n");
                    break;
            case RPC2_STRUCT_TAG: {
                    VAR **field;
                    char *newprefix;

                    if (var->array != NIL) {
                        fprintf(where, "\n     for(%s = 0; %s < ", _iterate, _iterate);
                        fprintf(where, "*%s; %s ++) {\n", var->array, _iterate);
                        newprefix = concat3elem(prefix, var->name, suffix);
                    } else {
                        newprefix = concat(prefix, var->name);
                    }
                    fprintf(where, "    if (unpack_struct_%s(buf, &%s))\n", var->type->name, newprefix);
                    fprintf(where, "        return -1;\n");
                    free(newprefix);
                    if (var->array != NIL)
                        fprintf(where, "    }\n\n");
            }
            break;
            case RPC2_ENUM_TAG:
                    fprintf(where, "    if (unpack_int(buf, (RPC2_Integer *)&%s))\n", name);
                    fprintf(where, "        return -1;\n");
                    break;
            default:
                    printf("RP2GEN: Unrecognized tag: %d\n", var->type->type->tag);
                    exit(1);
    }
    free(suffix);
    free(name);
}

void print_pack_var(char* prefix, VAR* var, FILE *where) {
    extern char* concat();
    char* name, *suffix;
    MODE mode;
    char _iterate[] = "_iterate";

    name = concat(prefix, var->name);
    suffix = concat3elem("[", _iterate, "]");
    mode = var->mode;
    switch(var->type->type->tag) {
            case RPC2_INTEGER_TAG:
                    fprintf(where, "    if (pack_int(buf, %s))\n", name);
                    fprintf(where, "        return -1;\n");
                    break;
            case RPC2_UNSIGNED_TAG:
                    fprintf(where, "    if (pack_unsigned(buf, %s))\n", name);
                    fprintf(where, "        return -1;\n");
                    break;
            case RPC2_BYTE_TAG:
                    if (var->type->bound != NIL) {
                        fprintf(where, "    if (pack_bound_bytes(buf, (char *)%s, %s))\n", name, var->type->bound);
                        fprintf(where, "        return -1;\n");
                    } else {
                        fprintf(where, "    if (pack_unbound_bytes(buf, %s))\n", name);
                        /* in struct, the following condition is not possible */
                        //if (who == RP2_CLIENT && mode == IN_OUT_MODE)
                        //    fputs("*", where);
                        fprintf(where, "        return -1;\n");
                    }
                    break;
            case RPC2_STRING_TAG:
                    fprintf(where, "    if (pack_string(buf, (char *)%s))\n", name);
                    fprintf(where, "        return -1;\n");
                    break;
            case RPC2_COUNTEDBS_TAG:
                    fprintf(where, "    if (pack_countedbs(buf, &(%s)))\n", name);
                    fprintf(where, "        return -1;\n");
                    break;
            case RPC2_BOUNDEDBS_TAG:
                    fprintf(where, "    if (pack_boundedbs(buf, &(%s)))\n", name);
                    fprintf(where, "        return -1;\n");
                    break;
            case RPC2_BULKDESCRIPTOR_TAG:
                    break;
            case RPC2_ENCRYPTIONKEY_TAG:
                    fprintf(where, "    if (pack_encryptionKey(buf, (char *)%s))\n", name);
                    fprintf(where, "        return -1;\n");
                    break;
            case RPC2_DOUBLE_TAG:
                    fprintf(where, "    if (pack_double(buf, %s))\n", name);
                    /* in struct, the following condition is not possible */
                    //if (who == RP2_CLIENT && mode == IN_OUT_MODE)
                    //    fputs("*", where);
                    fprintf(where, "        return -1;\n");
                    break;
            case RPC2_STRUCT_TAG: {
                    VAR **field;
                    char *newprefix;

                    if (var->array != NIL) {
                        fprintf(where, "\n     for(%s = 0; %s < ", _iterate, _iterate);
                        fprintf(where, "*%s; %s ++) {\n", var->array, _iterate);
                        newprefix = concat3elem(prefix, var->name, suffix);
                    } else {
                        newprefix = concat(prefix, var->name);
                    }
                    fprintf(where, "    if (pack_struct_%s(buf, &%s))\n", var->type->name, newprefix);
                    fprintf(where, "        return -1;\n");
                    free(newprefix);
                    if (var->array != NIL)
                        fprintf(where, "    }\n\n");
                }
        	break;
            case RPC2_ENUM_TAG:
                fprintf(where, "    if (pack_int(buf, %s))\n", name);
                fprintf(where, "        return -1;\n");
                break;
            default:
                printf("RP2GEN: Unrecognized tag: %d\n", var->type->type->tag);
                exit(1);
    }
    free(suffix);
    free(name);
}

static void print_type(RPC2_TYPE *t, FILE *where, char *name)
/* used to label struct typedefs */
{
    switch (t->tag) {
	case RPC2_INTEGER_TAG:
	case RPC2_UNSIGNED_TAG:
	case RPC2_BYTE_TAG:
	case RPC2_STRING_TAG:
	case RPC2_COUNTEDBS_TAG:
	case RPC2_BOUNDEDBS_TAG:
	case RPC2_BULKDESCRIPTOR_TAG:
	case RPC2_ENCRYPTIONKEY_TAG:
	case RPC2_DOUBLE_TAG: printf("RP2GEN [can't happen]: impossible type in PRINT_TYPE: %d\n", t->tag);
					abort();
	case RPC2_STRUCT_TAG:		{
					    VAR **v;
					    fprintf(where, "struct %s {\n", name);
					    for (v=t->fields.struct_fields; *v!=NIL; v++) {
						if (!legal_struct_fields[(int32_t) (*v)->type->type->tag]) {
						    printf("RP2GEN: illegal type for RPC2_Struct field: %s\n", (*v)->name);
						    exit(1);
						}
                                                fprintf(where, "    ");
						print_var(*v, where);
                                                fprintf(where, ";\n");
					    }
                                            fprintf(where, "}");
					}
					break;
	case RPC2_ENUM_TAG:		{
					    ENUM **id;
					    rp2_bool first;
                                            fprintf(where, "enum{");
					    for (id=t->fields.values, first=RP2_TRUE; *id!=NIL; id++) {
						if (!first)
                                                    fprintf(where, ",");
						else
						    first = RP2_FALSE;
						fprintf(where, " %s=%s", (*id)->name, (*id)->rep);
					    }
                                            fprintf(where, " }");
					}
					break;

	default:			printf("RP2GEN: Unrecognized tag: %d\n", t->tag);
					exit(1);
    }
}

static void print_var(VAR *v, FILE *where)
{
    fprintf(where, "%s %s", v->type->name, v->name);
}

void copcodes(PROC *head, WHO who, FILE *where)
{
    int32_t next_opnum; /* op code number to use if one not explicitly specified */
    char *args, *def;
    VAR **var;
    char msg[100];


#define PUTPARMS()\
	fprintf(where, "RPC2_Handle cid");\
	for(var = head->formals; *var != NIL; var++) {\
	    fprintf(where, ", ");\
	    spit_parm(*var, RP2_CLIENT, where, RP2_TRUE);\
	}\

    macro_define(where);

    /* the  stubpredefined structs are all already defined in rpc2.h */
    //print_stubpredefined(where);

    fprintf(where, "\n/* Op codes and definitions */\n\n");

    /* Generate <subsystem>_ExecuteRequest() definition, if
	<subsystem> is defined; may not be if HeadersOnlyFlag is RP2_TRUE */
    if ( ! cplusplus ) {
        fprintf(where, "#ifdef __cplusplus\n");
        fprintf(where, "extern \"C\"{\n");
        fprintf(where, "#endif\n");
    }

    if (subsystem.subsystem_name) {
	fprintf(where, "long %s_ExecuteRequest(RPC2_Handle cid, RPC2_PacketBuffer *pb, SE_Descriptor *se);\n\n", subsystem.subsystem_name);
    }
    for (next_opnum = 1; head != NIL; head = head->thread)
	if (!head->new_connection) { /* Normal routine */

	    /* Output extern proc definitions */
	    if (client_prefix)
                fprintf(where, "long %s_%s(", client_prefix, head->name);
	    else
                fprintf(where, "long %s(", head->name);
	    PUTPARMS();
	    fprintf(where, ");\n");

	    if (server_prefix) {
		fprintf(where, "long %s_%s(", server_prefix, head->name);
		PUTPARMS();
		fprintf(where, ");\n");
	    }

	    /* Output other definitions */
	    head->op_code = concat(head->name, "_OP");
            if (head->op_number == -1) {
                sprintf(msg, "no opcode number specified for %s; using %d",
		      head->name, next_opnum);
                line = head->linenum; yywarn(msg);
                head->op_number = next_opnum;
                next_opnum += 1;
            }
            else {
                if (head->op_number < next_opnum) {
		/* We've encountered a procedure whose opcode number is lower
                   than expected; this is not necessarily an error since there
                   may not be duplicates;  we could do duplicate detection with
		   an extra data structure, but it's so much simpler to
                   just demand that opcode numbers be in increasing order;
                   note that gaps in the opcode numbers are fine, and cause
                   no trouble   (Satya, 1/7/98)
                */
                    sprintf(msg, "opcode number specified for %s too low\n",
			  head->name);
                    line = head->linenum; yyerror(msg);
	      }
              next_opnum = head->op_number + 1;
            }
	    args = concat(head->name, "_ARGS");
	    def = concat(head->name, "_PTR");
	    fprintf(where, "#define %s\t%d\n", head->op_code, head->op_number);
	    fprintf(where, "extern ARG %s[];\n", args);
	    fprintf(where, "#define %s\t%s\n\n", def, args);
	    free(args);
	    free(def);
	}
	else {/* New connection routine */

	    /* Output server side proc definition */
	    if (server_prefix)
                fprintf(where, "extern long %s_%s(", server_prefix, head->name);
	    else fprintf(where, "extern long %s(", head->name);
            fprintf(where, "RPC2_Handle cid, RPC2_Integer SideEffectType, RPC2_Integer SecurityLevel, RPC2_Integer EncryptionType, RPC2_Integer AuthType, RPC2_CountedBS *ClientIdent);\n");
	    head -> op_code = "RPC2_NEWCONNECTION";
	}
    if ( ! cplusplus ) {
        fprintf(where, "#ifdef __cplusplus\n");
	fprintf(where, "}\n");
	fprintf(where, "#endif\n");
    }

    if (subsystem.subsystem_name) {
	fprintf(where, "#define %sOPARRAYSIZE %d\n", subsystem.subsystem_name, next_opnum);
	fprintf(where, "\nextern CallCountEntry %s_CallCount[];\n", subsystem.subsystem_name);
	fprintf(where, "\nextern MultiCallEntry %s_MultiCall[];\n", subsystem.subsystem_name);
	fprintf(where, "\nextern MultiStubWork %s_MultiStubWork[];\n", subsystem.subsystem_name);
	fprintf(where, "\nextern long %s_ElapseSwitch;\n", subsystem.subsystem_name);
	fprintf(where, "\nextern long %s_EnqueueRequest;\n", subsystem.subsystem_name);
    }

}

cproc(head, who, where)
    PROC *head;
    WHO who;
    FILE *where;
{
	switch (who) {
	case RP2_CLIENT: client_procs(head, where);
		break;
	case RP2_SERVER: server_procs(head, where);
		break;
	case RP2_MULTI:	 multi_procs(head, where);
		break;
	case RP2_DUMP:   dump_procs(head,where);
		break;
	default:	printf("RP2GEN [can't happen]: Impossible WHO: %d\n", who);
			abort();
    }
}

static char length[] = "_length";
static char reqbuffer[] = "_reqbuffer";
static char rspbuffer[] = "_rspbuffer";
static char rpc2val[] = "_rpc2val";
static char rpc2tmpval[] = "_rpc2tmpval";
static char bd[] = "_bd";
static char cid[] = "_cid";
static char timeoutval[] = "_timeoutval";
static char timeout[] = "_timeout";
static char code[] = "_code";
static char iterate[] = "_iterate";
static char timestart[] = "_timestart";
static char timeend[] = "_timeend";


static void locals(FILE *where, const char *who)
{
    fprintf(where, "    long %s, %s, %s;\n", length, rpc2val, code);
    fprintf(where, "    RPC2_PacketBuffer *%s = NULL;\n", rspbuffer);
    fprintf(where, "    BUFFER data_buf = { .who = %s };\n", who);
    fprintf(where, "    BUFFER *bufptr = &data_buf;\n");
}

static void common(FILE *where)
{
    fprintf(where, "\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n");
    fprintf(where, "\n#include <sys/types.h>\n#include <netinet/in.h>\n#include <sys/time.h>\n");
    fprintf(where, "#include <string.h>\n");
    fprintf(where, "#include <unistd.h>\n#include <stdlib.h>\n\n");
    fprintf(where, "\n#ifdef __cplusplus\n}\n#endif\n");

    fprintf(where, "\n#define _PAD(n)\t((((n)-1) | 3) + 1)\n");
}

static void client_procs(PROC *head, FILE *where)
{
    /* Generate preliminary stuff */
    common(where);
    version_check(where);
    WhatAmIDoing = INCLIENTS;

    declare_CallCount(head, where);	/* user transparent log structure
					   int *_ElapseSwitch;
					   *_CallCount[]
					 */
    //print_inlineFunction(where);
    /* Now, generate procs */
    for (; head!=NIL; head=head->thread)
	if (!head->new_connection) one_client_proc(head, where);
}

static void one_client_proc(PROC *proc, FILE *where)
{
    VAR **parm;
    rp2_bool in_parms, out_parms;

    /* Output name */
    fprintf(where, "\nlong ");
    if (client_prefix != NIL) fprintf(where, "%s_", client_prefix);

    fprintf(where, "%s(RPC2_Handle %s", proc->name, cid);

    /* Now do parameter list and types */
    in_parms = RP2_FALSE;
    out_parms = RP2_FALSE;

    for (parm=proc->formals; *parm!=NIL; parm++) {
	fprintf(where, ", ");
	spit_parm(*parm, RP2_CLIENT, where, RP2_TRUE);
	switch ((*parm)->mode) {
	case IN_MODE:
	    in_parms = RP2_TRUE;
	    break;
	case OUT_MODE:
	    out_parms = RP2_TRUE;
	    break;
	case IN_OUT_MODE:
	    in_parms = RP2_TRUE;
	    out_parms = RP2_TRUE;
	    break;
	default:
	    printf("[RP2GEN [can't happen]: bad MODE: %d]\n", (*parm)->mode);
	    abort();
	}
	/* BoundedBS always packs the bounds */
	if ((*parm)->type->type->tag == RPC2_BOUNDEDBS_TAG)
	    in_parms = RP2_TRUE;
    }
    fprintf(where, ")\n");

    /* DO body */
    spit_body(proc, in_parms, out_parms, where);
}

static char *mode_names[4] = { "Can't happen", "IN", "OUT", "IN OUT" };

static int32_t deref_table[][4] = {

				  /* NONE */ /* IN */ /* OUT */ /* IN OUT */
	/* RPC2_Integer */	{	-2,	0,	1,	1 },
	/* RPC2_Unsigned */{	-2,	0,	1,	1 },
	/* RPC2_Byte */	{	-2,	0,	1,	1 },
	/* RPC2_String */	{	-2,	0,	1,	0 },  /* changed OUT from -1 to 1 */
	/* RPC2_CountedBS */{	-2,	1,	1,	1 },  /* changed OUT from -1 to 1 */
	/* RPC2_BoundedBS */{	-2,	1,	1,	1 },  /* changed OUT from -1 to 1 */
	/* RPC2_BulkDescriptor*/{	-2,	-1,	-1,	1 },
	/* RPC2_EncryptionKey */{	-2,	0,	1,	1 },
	/* RPC2_Struct */	{	-2,	1,	1,	1 },
	/* RPC2_Enum */		{	-2,	0,	1,	1 },
	/* RPC2_Double */	{	-2,	0,	1,	1 }
};

static void spit_parm(VAR *parm, WHO who, FILE *where, rp2_bool header)
/* type info for c++ header? */
{
    ENTRY *type;
    int32_t levels;

    if (!header)    /* output mode info for parm lists */
	fprintf(where, "    /*%s*/\t", mode_names[(int32_t) parm->mode]);

    /* Now output appropriate type */
    type = parm->type;
    fprintf(where, "%s ", type->name);

    /* Output appropriate levels of referencing */
    if (type -> bound == NIL) {
	levels = deref_table[(int32_t) type->type->tag][(int32_t) parm->mode];
	if (who == RP2_SERVER && levels > 0) levels--;
	switch (levels) {
            case -2:	puts("RP2GEN [can't happen]: impossible MODE for variable");
			abort();
            case -1:	printf("RP2GEN: usage & type combination illegal for parameter %s\n", parm->name);
			exit(1);
            case 2:	fprintf(where, "*");
            case 1:	if (parm->array == NULL) fprintf(where, "*");
            case 0:	break;
	}
    }

    if (header) {
        fprintf(where, "%s", parm->name);
        if (parm->array != NIL)
            fprintf(where, "[]");
    } else {
        if (parm->array != NIL)
            if (who == RP2_SERVER)
                fprintf(where, "*%s = NULL;\n", parm->name);
            else
                fprintf(where, "%s[];\n", parm->name);
        else
            fprintf(where, "%s;\n", parm->name);
    }
}

static void for_limit(VAR *parm, WHO who, FILE *where)
{
    int32_t levels;

    /* Output appropriate levels of referencing */
    levels = deref_table[(int32_t)RPC2_INTEGER_TAG][(int32_t) parm->mode];
    if (who == RP2_SERVER && levels > 0) levels--;
    switch (levels) {
            case -2:	puts("RP2GEN [can't happen]: impossible MODE for variable");
			abort();
            case -1:	printf("RP2GEN: usage & type combination illegal for array suffix %s\n", parm->array);
			exit(1);
            case 2:	printf("RP2GEN: [can't happen]: array suffix %s\n", parm->array);
			exit(1);
            case 1:	fprintf(where, "*");
            case 0:	fprintf(where, "%s", parm->array);
	                break;
	    }
}


static void spit_body(PROC *proc, rp2_bool in_parms, rp2_bool out_parms, FILE *where)
{
    VAR **parm;
    rp2_bool first, array_parms;
    char *has_bd;

    /* Declare locals */
    fprintf(where, "{\n");
    locals(where, "RP2_CLIENT");

    /* client specific local variables */
    array_parms = RP2_FALSE;
    for (parm=proc->formals; *parm!=NIL; parm++) {
        if ((*parm)->array != NIL)
	    array_parms = RP2_TRUE;
    }
    if (array_parms)
        fprintf(where, "    long %s;\n", iterate);
    fprintf(where, "    struct timeval %s, %s;\n", timestart, timeend);
    /* Packet Buffer */
    fprintf(where, "    RPC2_PacketBuffer *%s = NULL;\n", reqbuffer);
    if (proc->timeout == NIL && !subsystem.timeout != NIL)
	fprintf(where, "    struct timeval *%s;\n", timeout);
    else fprintf(where, "    struct timeval %s, *%s;\n", timeoutval, timeout);

    /* note end of buffer */
    fprintf(where, "    int opengate = 0;\n");

    /* Generate code for START_ELAPSE */
    fprintf(where, "\n");
    fprintf(where, "    /* START_ELAPSE */\n");
    fprintf(where, "    %s_CallCount[%d].countent++;\n", subsystem.subsystem_name, proc->op_number);
    fprintf(where, "    if (%s_ElapseSwitch) {\n", subsystem.subsystem_name);
    fprintf(where, "\tgettimeofday(&_timestart, NULL);\n\topengate = 1;\n");
    fprintf(where, "    }\n\n");

    /* Compute buffer size */
    fprintf(where, "    %s = ", length);
    has_bd = NIL;
    if (in_parms) {
	for (parm=proc->formals, first=RP2_TRUE; *parm!=NIL; parm++)
	    if ((*parm)->mode != OUT_MODE ||
		(*parm)->type->type->tag == RPC2_BOUNDEDBS_TAG) {
		if ((*parm)->type->type->tag == RPC2_BULKDESCRIPTOR_TAG)
		    has_bd = (*parm) -> name;
                if (!first) fprintf(where, "+"); else first = RP2_FALSE;
		if ((*parm)->array == NIL) {
		    print_size(RP2_CLIENT, *parm, "", where);
		} else {
                    fprintf(where, "0");
		}
	    }
    } else {
        fprintf(where, "0");
    }

    fprintf(where, ";\n");

    if (in_parms) {
	for (parm=proc->formals; *parm!=NIL; parm++)
	    if ((*parm)->mode != OUT_MODE ||
		(*parm)->type->type->tag == RPC2_BOUNDEDBS_TAG)
		if ((*parm)->array != NIL) {
		    fprintf(where, "    /* %s of %s */\n", length, (*parm)->name);
		    fprintf(where, "    for ( %s = 0; %s < ", iterate, iterate);
		    for_limit(*parm, RP2_CLIENT, where);
                    fprintf(where, "; %s++)\n", iterate);
		    fprintf(where, "        %s += ", length);
		    array_print_size(RP2_CLIENT, *parm, "", where);
                    fprintf(where, ";\n");
		}
    }

    /* Get large enough buffer */
    fprintf(where, "    %s = RPC2_AllocBuffer(%s, &%s);\n", rpc2val, length, reqbuffer);
    fprintf(where, "    if (%s != RPC2_SUCCESS) return %s;\n", rpc2val, rpc2val);


	/* Now, do the packing */
       fprintf(where, "\n    /* Pack arguments */\n");
        fprintf(where, "    bufptr->buffer = (char *)%s->Body;\n", reqbuffer);
        fprintf(where, "    bufptr->eob = (char *)%s + %s->Prefix.BufferSize;\n", reqbuffer, reqbuffer);
    if (in_parms) {
       fprintf(where, "\n    %s = %s;\n\n", rspbuffer, reqbuffer);

	for (parm=proc->formals; *parm!=NIL; parm++)
	    if ((*parm)->mode != OUT_MODE ||
		(*parm)->type->type->tag == RPC2_BOUNDEDBS_TAG)
               pack(RP2_CLIENT, *parm, "", where);

        fprintf(where, "\n    %s = NULL;\n", rspbuffer);
    }

    /* Generate RPC2 call */
    fprintf(where, "\n    /* Generate RPC2 call */\n");
    fprintf(where, "    %s->Header.Opcode = %s;\n", reqbuffer, proc->op_code);
    /* Set up timeout */
    fprintf(where, "    ");
    set_timeout(proc, where);
    fprintf(where, "    %s = RPC2_MakeRPC(%s, %s, %s, &%s, %s, %s_EnqueueRequest);\n",
	    rpc2val, cid, reqbuffer, has_bd != NIL ? has_bd : "NULL", rspbuffer, timeout, subsystem.subsystem_name);
    fprintf(where, "    RPC2_FreeBuffer(&%s);\n", reqbuffer);
    fprintf(where, "    if (%s != RPC2_SUCCESS) {\n\tRPC2_FreeBuffer(&%s);\n\treturn %s;\n    }\n", rpc2val, rspbuffer, rpc2val);
    fprintf(where, "    if (%s->Header.ReturnCode == RPC2_INVALIDOPCODE) {\n\tRPC2_FreeBuffer(&%s);\n\treturn RPC2_INVALIDOPCODE;\n    }\n", rspbuffer, rspbuffer);

    /* Unpack arguments */
       fprintf(where, "\n    /* Unpack arguments */\n");
        fprintf(where, "    bufptr->buffer = (char *)%s->Body;\n", rspbuffer);
        fprintf(where, "    bufptr->eob = (char *)%s + %s->Prefix.LengthOfPacket + \n\t\t\tsizeof(struct RPC2_PacketBufferPrefix);\n", rspbuffer, rspbuffer);
    if (out_parms) {
	for (parm=proc->formals; *parm!=NIL; parm++)
           if ((*parm)->mode != IN_MODE) unpack(RP2_CLIENT, *parm, "", where);
    }
    /* Optionally translate OS independent errors back */
    if ( neterrors  ) {
        fprintf(where, "    %s = RPC2_R2SError(%s->Header.ReturnCode);\n", code, rspbuffer);
    } else {
        fprintf(where, "    %s = %s->Header.ReturnCode;\n", code, rspbuffer);
    }
    /* Throw away response buffer */
    fprintf(where, "    RPC2_FreeBuffer(&%s);\n", rspbuffer);

    /* Generate code for END_ELAPSE */
    fprintf(where, "\n");
    fprintf(where, "    /* END_ELAPSE */\n");
    fprintf(where, "    if (opengate) {\n");
    fprintf(where, "	gettimeofday(&_timeend, NULL);\n");
    fprintf(where, "	%s_CallCount[%d].tsec += _timeend.tv_sec - _timestart.tv_sec;\n", subsystem.subsystem_name, proc->op_number);
    fprintf(where, "	%s_CallCount[%d].tusec += _timeend.tv_usec - _timestart.tv_usec;\n", subsystem.subsystem_name, proc->op_number);

    fprintf(where, "	if (%s_CallCount[%d].tusec < 0) {\n", subsystem.subsystem_name, proc->op_number);
    fprintf(where, "	    %s_CallCount[%d].tusec += 1000000;\n", subsystem.subsystem_name, proc->op_number);
    fprintf(where, "	    %s_CallCount[%d].tsec--;\n", subsystem.subsystem_name, proc->op_number);

    fprintf(where, "	} else if (%s_CallCount[%d].tusec >= 1000000) {\n", subsystem.subsystem_name, proc->op_number);
    fprintf(where, "	    %s_CallCount[%d].tusec -= 1000000;\n", subsystem.subsystem_name, proc->op_number);
    fprintf(where, "	    %s_CallCount[%d].tsec++;\n", subsystem.subsystem_name, proc->op_number);
    fprintf(where, "	}\n");
    fprintf(where, "	%s_CallCount[%d].counttime++;\n", subsystem.subsystem_name, proc->op_number);
    fprintf(where, "    }\n");
    fprintf(where, "    %s_CallCount[%d].countexit++;\n\n", subsystem.subsystem_name, proc->op_number);

    /* Quit */
    fprintf(where, "    return %s;\n", code);

    if (buffer_checked) {
	fprintf(where, BUFFEROVERFLOW_END
		"    RPC2_FreeBuffer(&%s);\n    return RPC2_BADDATA;\n",
		rspbuffer);
    }
    buffer_checked = 0;

    /* Close off routine */
    fprintf(where, "}\n");
}

static char *field_name(parm, prefix)
    VAR *parm;
    char *prefix;
{
    return concat3elem(prefix, parm->name, parm->mode == NO_MODE ? "." : "->");
}

static char *field_name2(parm, prefix, suffix)
    VAR *parm;
    char *prefix;
    char *suffix;
{
    char *array, *pre_array, *result;
    array = concat(parm->name, suffix);
    pre_array = concat(prefix, array);
    if (parm->array != NIL)
	result = concat(pre_array, ".");
    else
	result = concat(pre_array, parm->mode == NO_MODE ? "." : "->");
    free(pre_array);
    free(array);

    return result;
}

static void array_print_size(WHO who, VAR *parm, char *prefix, FILE *where)
{
    char *name, *suffix;
    MODE mode;

    mode = parm->mode;
    suffix = concat3elem("[", iterate, "]");
    switch (parm->type->type->tag) {
	case RPC2_STRUCT_TAG:		if (parm->array != NIL) { /* Array Type */
					    VAR **field;
					    rp2_bool first;
					    char *newprefix;
					/*  newprefix = (who == RP2_SERVER
								? concat(prefix, concat(parm->name, concat(suffix, ".")))
								: field_name2(parm, prefix, suffix));
					*/
                                        char *tmp = concat3elem(prefix, parm->name, suffix);
                                        newprefix = concat(tmp, ".");
                                        fprintf(where, "(");
                                        first = RP2_TRUE;
                                        for (field=parm->type->type->fields.struct_fields; *field!=NIL; field++) {
                                            if (!first) fprintf(where, "+"); else first = RP2_FALSE;
                                            print_size(who, *field, newprefix, where);
                                        }
                                        free(newprefix);
                                        free(tmp);
                                        fprintf(where, ")");
                                    }
                                    break;
	default:	                break;	/* Should add error routine M.K. */
    }
    free(suffix);
}

static void print_size(WHO who, VAR *parm, char *prefix, FILE *where)
{
    char *name, *select;

    name = concat(prefix, parm->name);
/*  In addition to the following check, mode should be examined       */
/*  select = (who == RP2_CLIENT ? "->" : ".");                            */
/*  I guess CountedBS and BoundedBS have not been used as elements of */
/*  RPC2_Struct.                                                 M.K. */
    select = ((who == RP2_CLIENT && parm->mode != NO_MODE) ? "->" : ".");
    switch (parm->type->type->tag) {
	case RPC2_BYTE_TAG:		/* Check for array */
  					if (parm->type->bound != NIL) {
                                            fprintf(where, "_PAD(%s)",
                                                    parm->type->bound);
					    break;
					} else
					    /* Fall through */
					    ;
	case RPC2_INTEGER_TAG:
	case RPC2_UNSIGNED_TAG:
        case RPC2_ENUM_TAG:		fprintf(where, "4");
					break;
        case RPC2_DOUBLE_TAG:		fprintf(where, "8");
					break;
	case RPC2_STRING_TAG:		fprintf(where, "4+_PAD(strlen((char *)%s)+1)", name);
					break;
        case RPC2_COUNTEDBS_TAG:	fprintf(where, "4+_PAD(");
					if ((parm->mode == OUT_MODE) && (who == RP2_CLIENT))
					    fprintf(where, "(*%s)", name);
					else
                                            fprintf(where, "%s", name);
					fprintf(where, "%sSeqLen)", select);
					break;
        case RPC2_BOUNDEDBS_TAG:
            fprintf(where, "8");
	    if ((who == RP2_CLIENT && parm->mode != OUT_MODE) ||
		(who == RP2_SERVER && parm->mode != IN_MODE))
		fprintf(where, "+_PAD(%s%sSeqLen)", name, select);
	    break;
        case RPC2_BULKDESCRIPTOR_TAG:	fprintf(where, "0");
					break;
        case RPC2_ENCRYPTIONKEY_TAG:	fprintf(where, "_PAD(RPC2_KEYSIZE)");
					break;
	case RPC2_STRUCT_TAG:		if (parm->array == NIL) { /* NOT Array Type */
					    VAR **field;
					    rp2_bool first;
					    char *newprefix;
/*	How could this ever have worked?    newprefix = field_name(parm, prefix); */
                                            char *tmp = concat(parm->name, ".");
					    newprefix = (who == RP2_SERVER ? concat(prefix, tmp)
								: field_name(parm, prefix));
                                            fprintf(where, "(");
                                            free(tmp);
					    first = RP2_TRUE;
					    for (field=parm->type->type->fields.struct_fields; *field!=NIL; field++) {
                                                if (!first) fprintf(where, "+"); else first = RP2_FALSE;
                                                print_size(who, *field, newprefix, where);
					    }
                                            fprintf(where, ")");
					}
					break;
	default:			printf("RP2GEN [can't happen]: impossible type tag: %d\n",
					       parm->type->type->tag);
    }
    free(name);
}

static void inc(char *what, char *by, FILE *where)
{
    fprintf(where, "    %s += _PAD(%s);\n", what, by);
}

static void inc4(char *what, FILE *where)
{
    fprintf(where, "    %s += 4;\n", what);
}

static inc8(what, where)
    char *what;
    FILE *where;
{
    fprintf(where, "    %s += 8;\n", what);
}

static void set_timeout(PROC *proc, FILE *where)
{
    if (proc->timeout == NIL && !subsystem.timeout != NIL) {
	fprintf(where, "%s = NULL;\n", timeout);
	return;
    }


    fprintf(where, "%s.tv_sec = ", timeoutval);
    if (proc->timeout != NIL)
        fprintf(where, "%s", proc->timeout);
    else
        fprintf(where, "%s", subsystem.timeout);

    fprintf(where, "; %s.tv_usec = 0; %s = &%s;\n", timeoutval, timeout, timeoutval);
}

static void pack(WHO who, VAR *parm, char *prefix, FILE *where)
{
    extern char *concat();
    char *name, *select, *suffix;
    MODE mode;

    name = concat(prefix, parm->name);
    mode = parm -> mode;
/*  The following sentence was modified. See print_size() for detailed information.  M.K.  */
    //select = ((who == RP2_CLIENT && mode != NO_MODE) ? "->" : ".");
    suffix = concat3elem("[", iterate, "]");
    switch (parm->type->type->tag) {
    case RPC2_INTEGER_TAG:
            buffer_checked = 1;
            fprintf(where, "    if (pack_int(bufptr, ");
            if (who == RP2_CLIENT && mode == IN_OUT_MODE) fprintf(where, "*");
            fprintf(where, "%s))\n", name);
            fprintf(where, "        goto bufferoverflow;\n");
	    break;
    case RPC2_UNSIGNED_TAG:
            buffer_checked = 1;
            fprintf(where, "    if (pack_unsigned(bufptr, ");
            if (who == RP2_CLIENT && mode == IN_OUT_MODE) fprintf(where, "*");
            fprintf(where, "%s))\n", name);
            fprintf(where, "        goto bufferoverflow;\n");
	    break;
    case RPC2_BYTE_TAG:
            buffer_checked = 1;
            if (parm->type->bound != NIL) {
                fprintf(where, "    if (pack_bound_bytes(bufptr, (char *)%s, (long)%s))\n", name, parm->type->bound);
                fprintf(where, "        goto bufferoverflow;\n");
	    }
	    else {
                fprintf(where, "    if (pack_unbound_bytes(bufptr, ");
                if (who == RP2_CLIENT && mode == IN_OUT_MODE) fprintf(where, "*");
		    fprintf(where, "%s))\n", name);
                fprintf(where, "        goto bufferoverflow;\n");
	    }
	    break;
    case RPC2_ENUM_TAG:
            buffer_checked = 1;
            fprintf(where, "    if (pack_int(bufptr, (RPC2_Integer) ");
            if (who == RP2_CLIENT && mode == IN_OUT_MODE) fprintf(where, "*");
            fprintf(where, "%s))\n", name);
            fprintf(where, "        goto bufferoverflow;\n");
	    break;
    case RPC2_DOUBLE_TAG:
            buffer_checked = 1;
            fprintf(where, "    if (pack_double(bufptr, ");
            if (who == RP2_CLIENT && mode == IN_OUT_MODE) fprintf(where, "*");
            fprintf(where, "%s))\n", name);
            fprintf(where, "        goto bufferoverflow;\n");
	    break;
    case RPC2_STRING_TAG:
            buffer_checked = 1;
            fprintf(where, "    if (pack_string(bufptr, (char *)%s))\n", name);
            fprintf(where, "        goto bufferoverflow;\n");
	    break;
    case RPC2_COUNTEDBS_TAG:
            buffer_checked = 1;
            fprintf(where, "    if (pack_countedbs(bufptr, ");
            if (!(who == RP2_CLIENT && mode != NO_MODE))
                fprintf(where, "&");
            fprintf(where, "%s))\n", name);
            fprintf(where, "        goto bufferoverflow;\n");
	    break;
    case RPC2_BOUNDEDBS_TAG:
            buffer_checked = 1;
            fprintf(where, "    if (pack_boundedbs(bufptr, ");
            if (!(who == RP2_CLIENT && mode != NO_MODE))
                fprintf(where, "&");
            fprintf(where, "%s))\n", name);
            fprintf(where, "        goto bufferoverflow;\n");
	    break;
    case RPC2_STRUCT_TAG:       {
            VAR **field;
	    char *newprefix;
            char *tmp = NULL;
            buffer_checked = 1;

	    /* Dynamic arrays are taken care of here. */
	    /* If parm->array isn't NULL, this struct is used as DYNArray. */
	    if (parm->array !=NIL) {
                fprintf(where, "\n    for(%s = 0; %s < ", iterate, iterate);
                for_limit(parm, who, where);
                fprintf(where, "; %s++) {\n", iterate);

                tmp = concat3elem(prefix, parm->name, suffix);
                newprefix = concat("&", tmp);
            } else {
                newprefix = concat(prefix, parm->name);
                if (!(who == RP2_CLIENT && mode != NO_MODE)) {
                    tmp = newprefix;
                    newprefix = concat("&", newprefix);
                }
            }
            fprintf(where, "    if (pack_struct_%s(bufptr, %s))\n", parm->type->name, newprefix);
            fprintf(where, "        goto bufferoverflow;\n");
            free(newprefix);
            if (tmp)
                free(tmp);
            if (parm->array !=NIL)
                fprintf(where, "    }\n\n");
    }
    break;
    case RPC2_ENCRYPTIONKEY_TAG:{
            buffer_checked = 1;
            fprintf(where, " if (pack_encryptionKey(bufptr, (char *)%s))\n", name);
            fprintf(where, "        goto bufferoverflow;\n");
    }
    break;

    case RPC2_BULKDESCRIPTOR_TAG:	break;
    default:			printf("RP2GEN [can't happen]: unknown type tag: %d\n",
				       parm->type->type->tag);
    abort();
    }
    free(name);
    free(suffix);
}

static void unpack(WHO who, VAR *parm, char *prefix, FILE *where)
 {
    char *name, *select, *suffix;
    MODE mode;

    name = concat(prefix, parm->name);
    mode = parm -> mode;
/*  The following sentence was modified. See print_size() for detailed information.  M.K.  */
    select = ((who == RP2_CLIENT && mode != NO_MODE) ? "->" : ".");
    suffix = concat3elem("[", iterate, "]");
    switch (parm->type->type->tag) {
    case RPC2_INTEGER_TAG:
            buffer_checked = 1;
            fprintf(where, "    if (unpack_int(bufptr, ");
            if (!(mode != NO_MODE && who == RP2_CLIENT)) fprintf(where, "&");
            fprintf(where, "%s))\n", name);
            fprintf(where, "        goto bufferoverflow;\n");
            break;
    case RPC2_UNSIGNED_TAG:
            buffer_checked = 1;
            fprintf(where, "    if (unpack_unsigned(bufptr, ");
            if (!(mode != NO_MODE && who == RP2_CLIENT)) fprintf(where, "&");
            fprintf(where, "%s))\n", name);
            fprintf(where, "        goto bufferoverflow;\n");
            break;
    case RPC2_BYTE_TAG:
            buffer_checked = 1;
            if (parm->type->bound != NIL) {
                fprintf(where, "    ");
                fprintf(where, "if (unpack_bound_bytes(bufptr, (unsigned char *)%s, %s))\n",
		        name, parm->type->bound);
                fprintf(where, "        goto bufferoverflow;\n");
            }
            else {
                fprintf(where, "    if (unpack_unbound_bytes(bufptr, (unsigned char *)");
                if (!(mode != NO_MODE && who == RP2_CLIENT)) fprintf(where, "&");
                fprintf(where, "%s))\n", name);
                fprintf(where, "        goto bufferoverflow;\n");
            }
            break;
    case RPC2_ENUM_TAG:
            buffer_checked = 1;
            fprintf(where, "    if (unpack_int(bufptr, (RPC2_Integer *)");
            if (!(mode != NO_MODE && who == RP2_CLIENT)) fprintf(where, "&");
            fprintf(where, "%s))\n", name);
            fprintf(where, "        goto bufferoverflow;\n");
            break;
    case RPC2_DOUBLE_TAG:
            buffer_checked = 1;
            fprintf(where, "    if (unpack_double(bufptr, ");
            if (!(mode != NO_MODE && who == RP2_CLIENT)) fprintf(where, "&");
            fprintf(where, "%s))\n", name);
            fprintf(where, "        goto bufferoverflow;\n");
	    break;
    case RPC2_STRING_TAG:		/* 1st get length */
            buffer_checked = 1;
            fprintf(where, "    if (unpack_string(bufptr, (unsigned char **)&(%s)))\n", name);
            fprintf(where, "        goto bufferoverflow;\n");
	    break;
    case RPC2_COUNTEDBS_TAG:
            buffer_checked = 1;
            if (who == RP2_CLIENT && mode != IN_MODE) {
                fprintf(stderr, "Client should not unpack countedbs!\n");
                abort();
            }
            if (who == RP2_SERVER) {
                /* Special hack */
                fprintf(where, "    if (unpack_countedbs(bufptr, &(%s)))\n", name);
                fprintf(where, "        goto bufferoverflow;\n");
                break;
            }
            fprintf(where, "    if (unpack_countedbs(bufptr, &(%s)))\n", name);
            fprintf(where, "        goto bufferoverflow;\n");
            break;
    case RPC2_BOUNDEDBS_TAG:
            buffer_checked = 1;
            if (who == RP2_CLIENT && mode != NO_MODE)
                fprintf(where, "    if (unpack_boundedbs(bufptr, %d, %s))\n", mode, name);
            else
                fprintf(where, "    if (unpack_boundedbs(bufptr, %d, &(%s)))\n", mode, name);


            fprintf(where, "        goto bufferoverflow;\n");
            break;

    case RPC2_BULKDESCRIPTOR_TAG:	break;
    case RPC2_STRUCT_TAG:       {
	    VAR **field;
	    char *newprefix;
            char *tmp = NULL;
            buffer_checked = 1;

	    /* Dynamic arrays are taken care of here. */
	    /* If parm->array isn't NULL, this struct is used as DYNArray. */
	    if (parm->array !=NIL) {
                if (who == RP2_CLIENT) {
                    fprintf(where, "    if (");
                    for_limit(parm, who, where);
                    fprintf(where, " > ");
                    fprintf(where, "%s)\n" BUFFEROVERFLOW, parm->arraymax);
                }
                fprintf(where, "    for(%s = 0; %s < ", iterate, iterate);
                for_limit(parm, who, where);
		fprintf(where, "; %s++) {\n", iterate);
                tmp = concat3elem(prefix, parm->name, suffix);
                newprefix = concat("&", tmp);
            } else {
                newprefix = concat(prefix, parm->name);
                if (!(mode != NO_MODE && who == RP2_CLIENT)) {
                    tmp = newprefix;
                    newprefix = concat("&", newprefix);
                }
            }
            fprintf(where, "    if (unpack_struct_%s(bufptr, %s))\n", parm->type->name, newprefix);
            fprintf(where, "        goto bufferoverflow;\n");

	    free(newprefix);
            if (tmp)
                    free(tmp);
	    if (parm->array !=NIL)
                    fprintf(where, "    }\n\n");
    }
    break;
    case RPC2_ENCRYPTIONKEY_TAG: {
            buffer_checked = 1;
            fprintf(where, "    if (unpack_encryptionKey(bufptr, (char *)%s))\n", name);
            fprintf(where, "        goto bufferoverflow;\n");
    }
    break;
    default:			printf("RP2GEN [can't happen]: unknown tag: %d\n", parm->type->type->tag);
    abort();
    }
    free(name);
    free(suffix);
}


static void server_procs(PROC *head, FILE *where)
{
    PROC *proc;

    /* Preliminary stuff */
    common(where);
    version_check(where);
    WhatAmIDoing = INSERVERS;


    /* Generate server unpacking routines */
    for (proc=head; proc!=NIL; proc=proc->thread) one_server_proc(proc, where);

    /* Generate ExecuteRequest routine */
    execute(head, where);
}

static void check_new_connection(PROC *proc)
{
    VAR **formals;
    int32_t len;

    /* Warn if timeout override specified */
    if (proc->timeout != NIL) puts("RP2GEN [warning]: TIMEOUT ignored on NEW CONNECTION procedure");

    /* Check argument types */
    for (formals = proc->formals, len=0; *formals!=NIL; formals++, len++) ;
    formals = proc->formals;
    if (len != 5 ||
	formals[0]->type->type->tag != RPC2_INTEGER_TAG || /* SideEffectType */
	formals[1]->type->type->tag != RPC2_INTEGER_TAG || /* SecurityLevel */
	formals[2]->type->type->tag != RPC2_INTEGER_TAG || /* EncryptionType */
	formals[3]->type->type->tag != RPC2_INTEGER_TAG || /* AuthType */
	formals[4]->type->type->tag != RPC2_COUNTEDBS_TAG) { /* ClientIdent */
	puts("RP2GEN: bad parameters for NEW_CONNECTION procedure");
	exit(1);
    }
}

static void one_server_proc(PROC *proc, FILE *where)
{
    VAR **formals;
    rp2_bool first, in_parms, out_parms, array_parms;

    /* If NEW CONNECTION proc, check parameters */
    if (proc->new_connection) check_new_connection(proc);

    /* Generate header */
    fprintf(where, "\nstatic RPC2_PacketBuffer *_");
    if (server_prefix != NIL) fprintf(where, "%s_", server_prefix);
    fprintf(where, "%s(RPC2_Handle %s, RPC2_PacketBuffer *%s, ",
	    proc->name, cid, reqbuffer);
    fprintf(where, "SE_Descriptor *%s)\n", bd);

    /* Generate body */
    fprintf(where, "{\n");

    /* declare locals */
    locals(where, "RP2_SERVER");

    /* Declare parms */
    in_parms = RP2_FALSE;
    out_parms = RP2_FALSE;
    array_parms = RP2_FALSE;
    for (formals=proc->formals; *formals!=NIL; formals++) {
	if ((*formals)->type->type->tag != RPC2_BULKDESCRIPTOR_TAG)
	    spit_parm(*formals, RP2_SERVER, where, RP2_FALSE);
	if ((*formals)->array != NIL)
	    array_parms = RP2_TRUE;
	switch ((*formals)->mode) {
	    case IN_MODE:	in_parms = RP2_TRUE;
				break;
	    case IN_OUT_MODE:	in_parms = RP2_TRUE;
	    case OUT_MODE:	out_parms = RP2_TRUE;
				break;
	    default:		printf("[RP2GEN (can't happen)]: unknown mode: %d\n", (*formals)->mode);
	}
	/* BoundedBS will at least send MaxSeqLen */
	if ((*formals)->type->type->tag == RPC2_BOUNDEDBS_TAG)
	    in_parms = RP2_TRUE;
    }

    if (array_parms) {
        fprintf(where, "    long %s;\n", iterate);
    }

    fprintf(where, "\n    /* Unpack parameters */\n");
    fprintf(where, "    bufptr->buffer = (char *)%s->Body;\n", reqbuffer);
    fprintf(where, "    bufptr->eob = (char *)%s + %s->Prefix.LengthOfPacket + \n\t\t\tsizeof(struct RPC2_PacketBufferPrefix);\n", reqbuffer, reqbuffer);

    if (in_parms) {
        fprintf(where, "\n    %s = %s;\n", rspbuffer, reqbuffer);
	for (formals=proc->formals; *formals!=NIL; formals++)
	    if ((*formals)->type->type->tag != RPC2_BULKDESCRIPTOR_TAG) {
	        /* As DYNArray always has IN mode parameter,
		   it is good place to allocate buffer */
	        /* Allocate dynamic array buffer */
	        if ((*formals)->array != NIL) {
		    alloc_dynamicarray(*formals, RP2_SERVER, where);
		}
	        if ((*formals)->mode != OUT_MODE ||
		    (*formals)->type->type->tag == RPC2_BOUNDEDBS_TAG) {
                   unpack(RP2_SERVER, *formals, "", where);
		}
	    }
        fprintf(where, "\n    %s = NULL;\n", rspbuffer);
    }
    fprintf(where, "    %s = %s;\n", bd, bd);

    /* Call the user's routine */
    fprintf(where, "\n    %s = ", code);
    if (server_prefix != NIL) fprintf(where, "%s_", server_prefix);
    fprintf(where, "%s(%s", proc->name, cid);
    for (formals=proc->formals; *formals!=NIL; formals++) {
        fprintf(where, ", ");
	pass_parm(*formals, where);
    }
    fprintf(where, ");\n");

    fprintf(where, "\n    %s = ", length);
    if (out_parms) {
	for (formals=proc->formals, first=RP2_TRUE; *formals!=NIL; formals++)
	if ((*formals)->mode != IN_MODE) {
                if (!first) fprintf(where, "+"); else first = RP2_FALSE;
	    if ((*formals)->array == NIL) {
		print_size(RP2_SERVER, *formals, "", where);
	    } else {
                    fprintf(where, "0");
	    }
	}
    } else
        fprintf(where, "0");

    fprintf(where, ";\n");

    if (out_parms) {
	for (formals=proc->formals; *formals!=NIL; formals++)
	    if ((*formals)->mode != IN_MODE)
		if ((*formals)->array != NIL) {
		    fprintf(where, "    /* %s of %s */\n", length, (*formals)->name);
		    fprintf(where, "    for ( %s = 0; %s < ", iterate, iterate);
		    for_limit(*formals, RP2_SERVER, where);
		    fprintf(where, "; %s++)\n", iterate);
		    fprintf(where, "        %s += ", length);
		    array_print_size(RP2_SERVER, *formals, "", where);
                    fprintf(where, ";\n");
		}
    }

    fprintf(where, "    %s = RPC2_AllocBuffer(%s, &%s);\n", rpc2val, length, rspbuffer);
    fprintf(where, "    if (%s != RPC2_SUCCESS) return NULL;\n", rpc2val);
    if ( neterrors ) {
    fprintf(where, "    %s->Header.ReturnCode = RPC2_S2RError(%s);\n",
	    rspbuffer, code);
    } else {
    fprintf(where, "    %s->Header.ReturnCode = %s;\n", rspbuffer, code);
    }

    /* Pack return parameters */
    fprintf(where, "\n    /* Pack return parameters */\n");
    fprintf(where, "    bufptr->buffer = (char *)%s->Body;\n", rspbuffer);
    fprintf(where, "    bufptr->eob = (char *)%s + %s->Prefix.BufferSize;\n", rspbuffer, rspbuffer);

    for (formals=proc->formals; *formals!=NIL; formals++) {
	TYPE_TAG tag;
	tag = (*formals)->type->type->tag;
	if (tag != RPC2_BULKDESCRIPTOR_TAG) {
	    if ((*formals)->mode != IN_MODE) {
               pack(RP2_SERVER, *formals, "", where);
	    }
	    if ((*formals)->array != NIL) {
	        free_dynamicarray(*formals, where);
	    }
        }
	if (tag == RPC2_BOUNDEDBS_TAG) free_boundedbs(*formals, where);
    }

    fprintf(where, "    return %s;\n", rspbuffer);

    if (buffer_checked)
    fprintf(where, BUFFEROVERFLOW_END "    return NULL;\n");
    buffer_checked = 0;

    /* Close routine */
    fprintf(where, "}\n");
}

static void free_boundedbs(VAR *parm, FILE *where)
{
    fprintf(where, "    if (%s.SeqBody) free((char *)%s.SeqBody);\n", parm->name, parm->name);
}

static void alloc_dynamicarray(VAR *parm, WHO who, FILE *where)
{
    MODE mode;

    mode = parm -> mode;
    /* parm->arraymax is used without any modification.                 *
     * it is valid, because this routine is used only in RP2_SERVER case,   *
     * and in RP2_SERVER case, no * is needed.                              */
    fprintf(where, "    if (%s > 0) {\n", (mode == IN_MODE)
	                                  ? parm->array : parm->arraymax);
    fprintf(where, "\t%s = (%s *)malloc(sizeof(%s)*(%s));\n",
	    parm->name, parm->type->name, parm->type->name,
	    (mode == IN_MODE) ? parm->array : parm->arraymax);
    fprintf(where, "\tif (%s == NULL) return NULL;\n    }\n", parm->name);
}

static void free_dynamicarray(VAR *parm, FILE *where)
{
    fprintf(where, "    if (%s) free((char *)%s);\n", parm->name, parm->name);
}

static void pass_parm(VAR *parm, FILE *where)
{
    MODE mode;

    mode = parm -> mode;
    switch (parm->type->type->tag) {
	case RPC2_BYTE_TAG:
	case RPC2_INTEGER_TAG:
	case RPC2_UNSIGNED_TAG:
	case RPC2_ENUM_TAG:
	case RPC2_DOUBLE_TAG:		if (parm->type->bound == NIL && (mode == OUT_MODE || mode == IN_OUT_MODE))
                                            fprintf(where, "&");
                                        fprintf(where, "%s", parm->name);
					break;
	case RPC2_STRING_TAG:		if (mode == OUT_MODE)
                                            fprintf(where, "&");
                                        fprintf(where, "%s", parm->name);
					break;
	case RPC2_COUNTEDBS_TAG:
	case RPC2_BOUNDEDBS_TAG:	fprintf(where, "&%s", parm->name);
					break;
        case RPC2_BULKDESCRIPTOR_TAG:	fprintf(where, "%s", bd);
					break;
        case RPC2_ENCRYPTIONKEY_TAG:	fprintf(where, "%s", parm->name);
					break;
	case RPC2_STRUCT_TAG:		fprintf(where, (parm->array == NIL) ? "&%s" : "%s", parm->name);
					break;
	default:			printf("RP2GEN [can't happen]: unknown type tag: %d\n",
					       parm->type->type->tag);
					abort();
    }
}

static void execute(PROC *head, FILE *where)
{
#if	__GNUC__ < 2
    extern int32_t strlen();
#endif
    extern char *copy();
    int32_t sawnewconn;

    fprintf(where, "\nlong %s_ExecuteRequest(RPC2_Handle %s, RPC2_PacketBuffer *%s, SE_Descriptor *%s)\n", subsystem.subsystem_name, cid, reqbuffer, bd);

    /* Body of routine */
    fprintf(where, "{\n    RPC2_PacketBuffer *%s;\n    long %s, %s;\n", rspbuffer, rpc2val, rpc2tmpval);
    fprintf(where, "\n    switch (%s->Header.Opcode) {\n", reqbuffer);
    sawnewconn = 0;

    /* Do case arms */
    for (; head!=NIL; head=head->thread) {
	fprintf(where, "\tcase %s:\n\t\t", head->op_code);
	fprintf(where, "%s = _", rspbuffer);
	if (server_prefix != NIL) fprintf(where, "%s_", server_prefix);
	fprintf(where, "%s(%s, %s, %s);\n\t\t", head->name, cid, reqbuffer, bd);
	if (!head->new_connection) {
            fprintf(where, "\t\tbreak;\n");
	} else {
	    sawnewconn = 1;
	    fprintf(where, "RPC2_FreeBuffer(&%s);\n", reqbuffer);
	    fprintf(where, "\t\tRPC2_FreeBuffer(&%s);\n", rspbuffer);
	    fprintf(where, "\t\treturn RPC2_Enable(%s);\n", cid);
	}
    }

    /* Add a default new connection routine */
    if (!sawnewconn)
	{
	fprintf(where, "\tcase RPC2_NEWCONNECTION:\n");
	fprintf(where, "\t\tRPC2_FreeBuffer(&%s);\n", reqbuffer);
	fprintf(where, "\t\treturn RPC2_Enable(%s);\n", cid);
	}


    /* Add default arm */
    fprintf(where, "\tdefault:\n");
    fprintf(where, "\t\tif (RPC2_AllocBuffer(0, &%s) != RPC2_SUCCESS) return(RPC2_FAIL);\n", rspbuffer);
    fprintf(where, "\t\t%s->Header.ReturnCode = RPC2_INVALIDOPCODE;\n", rspbuffer);


    /* Close off case */
    fprintf(where, "    }\n");

    /* Throw away request buffer, send response and discard response buffer */
    fprintf(where, "    %s = RPC2_FreeBuffer(&%s);\n", rpc2tmpval, reqbuffer);
    fprintf(where, "    %s = RPC2_SendResponse(%s, %s);\n", rpc2val, cid, rspbuffer);
    fprintf(where, "    if (%s == RPC2_SUCCESS)\n\t%s = %s;\n", rpc2val, rpc2val, rpc2tmpval);
    fprintf(where, "    %s = RPC2_FreeBuffer(&%s);\n", rpc2tmpval, rspbuffer);
    fprintf(where, "    if (%s == RPC2_SUCCESS)\n\t%s = %s;\n", rpc2val, rpc2val, rpc2tmpval);
    fprintf(where, "    return %s;\n", rpc2val);

    /* Close off routine */
    fprintf(where, "}\n");
}

/* spit out code to pretty print packets in tcpdump */
static print_dump(head, where)
    PROC *head;
    FILE *where;
{
#if	__GNUC__ < 2
    extern int32_t strlen();
#endif
    extern char *copy();
    int32_t sawnewconn;

    fprintf(where, "\nint %s_PrintOpcode(int opcode, int subsysid) {\n",
	    subsystem.subsystem_name);

    /* Body of routine */
    fprintf(where, "/*    Subsystem : %s */\n",subsystem.subsystem_name);
    fprintf(where, "\n    printf(\"%s:\");\n",subsystem.subsystem_name);
    fprintf(where, "\n    switch (opcode) {\n");
    sawnewconn = 0;

    /* Do case arms */
    for (; head!=NIL; head=head->thread) {
	fprintf(where, "\tcase %s:\n\t\t", head->op_code);
	fprintf(where, "printf(\"%s\");",head->op_code);
	fprintf(where, "\n\t\tbreak;\n");
    }

     /* Add default arm */
    fprintf(where, "\tdefault:\n");
    fprintf(where, "\t\tprintf(\"%%d\",opcode);\n");
    /* Close off case */
    fprintf(where, "    }\n");
    /* Close off routine */
    fprintf(where, "}\n");
}

static void dump_procs(PROC *head, FILE *where)
{
    /* Preliminary stuff */
    common(where);
    /*    version_check(where);
    WhatAmIDoing = NEITHER;
	*/
    print_dump(head,where);
}


static void multi_procs(PROC *head, FILE *where)
{
    PROC *proc;     /* temp for iteration to create arg descriptors */
    VAR **var;
    char *args;
    char *subname;
    int32_t arg = 1;	     /* argument number in order of appearance */


    /* Preliminary stuff */
    common(where);
    version_check(where);
    WhatAmIDoing = NEITHER;

    declare_MultiCall(head, where);	/* user transparent log structure
					   int *_ElapseSwitch;
					   *_CallCount[]
					 */
    declare_LogFunction(head, where);

    /* Generate argument descriptors for MakeMulti call */
    for(proc =  head; proc != NIL; proc = proc->thread) {
	args = concat(proc->name, "_ARGS");

	/* recursively write out any structure arguments */
	for(var = proc->formals; *var != NIL; var++, arg++) {
	    if ((*var)->type->type->tag == RPC2_STRUCT_TAG)
	       do_struct((*var)->type->type->fields.struct_fields,
                         proc->op_number, arg, 1, 1, where);
	}
	arg = 1;	/* reset argument counter */
	fprintf(where, "\nARG\t%s[] = {\n", args);
	for(var = proc->formals; *var != NIL; var++, arg++) {
	    fprintf(where, "\t\t{%s, %s, ", MultiModes[(int32_t)(*var)->mode], MultiTypes[(int32_t)(*var)->type->type->tag]);
	    pr_size(*var, where, RP2_TRUE, proc->op_number, arg);
	    fprintf(where, ", NULL, NULL},\n");
	}
	subname = subsystem.subsystem_name;
	/* RPC2_STRUCT_TAG in C_END definition is bogus */
	fprintf(where, "\t\t{%s, RPC2_STRUCT_TAG, 0, NULL, 0, &%s_startlog, &%s_endlog}\n", MultiModes[4], subname, subname);
	fprintf(where, "\t};\n");
	free(args);
	arg = 1; 	/* reset argument counter */
    }
}


static void pr_size(VAR *parm, FILE *where, rp2_bool TOP, int32_t proc, int32_t arg)
{
    switch (parm->type->type->tag) {
	case RPC2_BYTE_TAG:		/* Check for array */
  					if (parm->type->bound != NIL) {
					    fprintf(where, "%s", parm->type->bound);
					}
                                        else fprintf(where, "0");
					break;
					/* negative number indicates array */
					/* Fall through */
	case RPC2_INTEGER_TAG:
	case RPC2_UNSIGNED_TAG:
        case RPC2_ENUM_TAG:		fprintf(where, "4");
					break;
        case RPC2_DOUBLE_TAG:		fprintf(where, "8");
					break;
	case RPC2_STRING_TAG:
	case RPC2_COUNTEDBS_TAG:
	case RPC2_BOUNDEDBS_TAG:
                                        fprintf(where, "0");   /* calculated at runtime */
					break;
        case RPC2_BULKDESCRIPTOR_TAG:	fprintf(where, "0");
					break;
        case RPC2_ENCRYPTIONKEY_TAG:	fprintf(where, "_PAD(RPC2_KEYSIZE)");
					break;
	case RPC2_STRUCT_TAG:
					if (TOP)
                                                fprintf(where, "sizeof(%s)", parm->type->name);
                                        else fprintf(where, "0");
					if (TOP) {
                                            fprintf(where, ", STRUCT_%d_%d_1_1", proc, arg);
                                            if (parm->array != NIL)
                                                fprintf(where, ", 1");
                                            else
                                                fprintf(where, ", 0");
					}
					break;


	default:			printf("RP2GEN [can't happen]: impossible type tag: %d\n",
					       parm->type->type->tag);
    }
    if (TOP && parm->type->type->tag != RPC2_STRUCT_TAG)
    	fprintf(where, ", NULL, 0");
}


static void do_struct(VAR **fields, int32_t proc, int32_t arg, int32_t level, int32_t cur_struct, FILE *where)
 {
   VAR **field;
   int32_t structs = 0;

   for(field = fields; *field != NIL; field++) {
	if ((*field)->type->type->tag == RPC2_STRUCT_TAG) structs++;
   }
   if (structs != 0) {
	for (--field; field >= fields; field--) {
	   if ((*field)->type->type->tag == RPC2_STRUCT_TAG)
		do_struct((*field)->type->type->fields.struct_fields, proc, arg, level + 1, structs--, where);
	}
   }

   fprintf(where, "\nstatic ARG\tSTRUCT_%d_%d_%d_%d[] = {\n", proc, arg, level, cur_struct);
   for(field = fields; *field != NIL; field++) {
     fprintf(where, "\t\t{%s, %s, ", MultiModes[0], MultiTypes[(int32_t)(*field)->type->type->tag]);
     pr_size(*field, where, RP2_FALSE, proc, arg);
     if((*field)->type->type->tag == RPC2_STRUCT_TAG) {
	fprintf(where, ", STRUCT_%d_%d_%d_%d, 0", proc, arg, level + 1, ++structs);
     } else
        fprintf(where, ", NULL, 0");
     fprintf(where, ", NULL, NULL},\n");
   }
   fprintf(where, "\t\t{%s, 0, 0, NULL, 0, NULL, NULL}\n\t};\n", MultiModes[4]);
 }

static void macro_define(FILE *where)
{
    char *subname;
    subname = subsystem.subsystem_name;
    if (!subname) return; /* Must be a HeadersOnly case */

    fprintf(where, "\n#define %s_HEAD_VERSION\t%d\n", subname, versionnumber);
}

static void version_check(FILE *where)
{
    fprintf(where, "\n\n#if (%s_HEAD_VERSION != %d)", subsystem.subsystem_name, versionnumber);
    fprintf(where, "\n; char *NOTE[] = ");
    fprintf(where, "CAUTION_______________________________________!!!");
    fprintf(where, "VERSION_IS_INCONSISTENT_WITH_HEADER_FILE______!!!");
    fprintf(where, "PLEASE_CHECK_YOUR_FILE_VERSION________________;");
    fprintf(where, "\n#endif\n");
}


static void declare_CallCount(PROC *head, FILE *where)
{
   int i, last_op = -1;
   fprintf(where, "\nlong %s_ElapseSwitch = 0;\n", subsystem.subsystem_name);
   fprintf(where, "\nlong %s_EnqueueRequest = 1;\n", subsystem.subsystem_name);

   fprintf(where, "\nCallCountEntry %s_CallCount[] = {\n", subsystem.subsystem_name);

   for ( ; head!=NIL; ) {
       if (!head->new_connection) {

           /* insert dummy entries for not implemented operations */
           for (i = last_op+1; i < head->op_number; i++)
                fprintf(where, "\t/* dummy */\t\t{(RPC2_String)\"dummy\", 0, 0, 0, 0, 0},\n");
           last_op = head->op_number;

	   fprintf(where, "\t/* %s_OP */\t{(RPC2_String)\"%s\", 0, 0, 0, 0, 0}", head->name, head->name);
	   /* The above (RPC2_String) avoids compiler's annoyance warning */

	   if ( (head=head->thread) != NIL )
                fprintf(where, ",\n");

       } else
	   head = head->thread;
   }

   fprintf(where, "\n};\n");

}

static void declare_MultiCall(PROC *head, FILE *where)
{
   PROC *threads;
   int   i, last_op = -1;

   fprintf(where, "\nMultiCallEntry %s_MultiCall[] = {\n", subsystem.subsystem_name);
   for (threads = head ; threads!=NIL; ) {
       if (!threads->new_connection) {

           /* add dummy entries for not implemented operations */
           for (i = last_op+1; i < threads->op_number; i++)
                fprintf(where, "\t/* dummy */\t\t{(RPC2_String)\"dummy\", 0, 0, 0, 0, 0, 0},\n");
           last_op = threads->op_number;

	   fprintf(where, "\t/* %s_OP */\t{(RPC2_String)\"%s\", 0, 0, 0, 0, 0, 0}", threads->name, threads->name);
	   /* The above (RPC2_String) avoids compiler's annoyance warning */

	   if ( (threads=threads->thread) != NIL )
                fprintf(where, ",\n");

       } else
	   threads = threads->thread;
   }
   fprintf(where, "\n};\n");

   fprintf(where, "\nMultiStubWork %s_MultiStubWork[] = {\n", subsystem.subsystem_name);
   last_op = -1;
   for (threads = head ; threads!=NIL; ) {
       if (!threads->new_connection) {

           /* insert dummy entries for not implemented operations */
           for (i = last_op+1; i < threads->op_number; i++)
                fprintf(where, "\t/* dummy */\t\t{0, 0, 0},\n");
           last_op = threads->op_number;

	   fprintf(where, "\t/* %s_OP */\t{0, 0, 0}", threads->name);

	   if ( (threads=threads->thread) != NIL )
                fprintf(where, ",\n");

       } else
	   threads = threads->thread;
   }
   fprintf(where, "\n};\n");

}

static void declare_LogFunction(PROC *head, FILE *where)
{
    char *array;
    char *work;
    char *subname;

    subname = subsystem.subsystem_name;

    array = concat(subname, "_MultiCall[op]");
    work = concat(subname, "_MultiStubWork[op]");

    /* define startlog function */
    fprintf(where, "\nvoid %s_startlog(long op)\n", subname);
    fprintf(where, "{\n");
    fprintf(where, "    struct timeval timestart;\n");
    fprintf(where, "\n    ++%s.countent;\n", array);
    fprintf(where, "    if ( %s_%s[0].opengate ) {\n", subname, "MultiStubWork");
    fprintf(where, "        gettimeofday(&timestart, NULL);\n");
    fprintf(where, "        %s.tsec = timestart.tv_sec;\n", work);
    fprintf(where, "        %s.tusec = timestart.tv_usec;\n", work);
    fprintf(where, "        %s.opengate = 1;\n", work);
    fprintf(where, "    } else %s.opengate = 0;\n", work);
    fprintf(where, "}\n");

    /* define endlog function */
    fprintf(where, "\nvoid %s_endlog(long op, RPC2_Integer many, RPC2_Handle *cidlist, RPC2_Integer *rclist)\n", subname);
    fprintf(where, "{\n");
    fprintf(where, "    struct timeval timeend;\n");
    fprintf(where, "    long i, timework, istimeouted, hosts;\n");
    fprintf(where, "\n    istimeouted = hosts = 0;\n");
    fprintf(where, "    if ( rclist == 0 ) return;\n");
    fprintf(where, "    for ( i = 0; i < many ; i++) {\n");
    fprintf(where, "        if ( cidlist[i] != 0 && rclist[i] == RPC2_TIMEOUT ) istimeouted = 1;\n");
    fprintf(where, "        if ( cidlist[i] != 0 && (rclist[i] >= 0) ) hosts++;\n");
    fprintf(where, "    }\n");
    fprintf(where, "    if ( istimeouted == 0 ) {\n");
    fprintf(where, "        ++%s.countexit;\n", array);
    fprintf(where, "        if ( %s.opengate ) {\n", work);
    fprintf(where, "            gettimeofday(&timeend, NULL);\n");
    fprintf(where, "            timework = (%s.tusec += (timeend.tv_sec-%s.tsec)*1000000+(timeend.tv_usec-%s.tusec))/1000000;\n", array, work, work);
    fprintf(where, "            %s.tusec -= timework*1000000;\n", array);
    fprintf(where, "            %s.tsec += timework;\n", array);
    fprintf(where, "            ++%s.counttime;\n", array);
    fprintf(where, "            %s.counthost += hosts;\n", array);
    fprintf(where, "        }\n");
    fprintf(where, "    }\n");
    fprintf(where, "}\n");
    free(array);
    free(work);

}
