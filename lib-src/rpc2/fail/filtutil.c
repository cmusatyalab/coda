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

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "coda_assert.h"
#include "ports.h"
#include "filtutil.h"



/* Maintain state of currently open connection, if there is one */
static RPC2_Handle cid;
static int maxFilterID[2];

void PrintError(char *, int);


/***** Filter structures / functions *****/

/* Predefined matrices used by create_filter() internally referred to by
   setting filter_type */
const FailFilter filter_templates[] = {
  {			/* ISOLATE */
    -1, -1, -1, -1,	/* Specify all IP addresses */
    -1,			/* All colors */
    0,			/* Apply this filter at the top */
    0, 65535,		/* Packet lengths */
    0,			/* Probability 0 (blocks any packets) */
    0			/* Speed zero */
  },
  {			/* PARTITION */
    0, 0, 0, 0,		/* Specify IP addresses (CHANGE) */
    -1,			/* All colors */
    0,			/* Apply this filter at the top */
    0, 65535,		/* Packet lengths */
    0,			/* Probability 0 (blocks any packets) */
    0			/* Speed zero */
  },
  {			/* SERVER */
    0, 0, 0, 0,		/* Specify IP addresses (CHANGE) */
    -1,			/* All colors */
    0,			/* Apply this filter at the top */
    0, 65535,		/* Packet lengths */
    MAXPROBABILITY,	/* Max probability (allows all packets) */
    MAXNETSPEED		/* Full speed */
  },
  {			/* JOIN */
    0, 0, 0, 0,		/* Specify IP addresses (CHANGE) */
    -1,			/* All colors */
    0,			/* Apply this filter at the top */
    0, 65535,		/* Packet lengths */
    MAXPROBABILITY,	/* Max probability (allows all packets) */
    MAXNETSPEED		/* Full speed */
  }
};

/* Allocates and sets the data to a filter pointer based on the requested
   type */
void create_filter(filter_type type, FailFilter **filter)
{
  *filter = NULL;

  if (type > MAXFILTERTYPE)
    return;

  if (*filter = (FailFilter *)malloc(sizeof(FailFilter)))
    bcopy(&filter_templates[type], *filter, sizeof(FailFilter));
}

/* Frees up the memory associated with a filter made by create_filter */
void destroy_filter(FailFilter *filter)
{
  free(filter);
}

/* Changes the ip address fields within a filter.  Ideally this is all
   that should need to be changed, and even it does not need to be set
   all the time */
int set_filter_host(target_t target, FailFilter *filter)
{
  int ip1, ip2, ip3, ip4;

  if (target_to_ip(target, &ip1, &ip2, &ip3, &ip4))
    return -1;

  filter->ip1 = ip1;
  filter->ip2 = ip2;
  filter->ip3 = ip3;
  filter->ip4 = ip4;

  return 0;
}

/* Insert a filter at the requested position on the currently connected
   target */
int insert_filter(FailFilter *filter, int which)
{
  int i, rc, side, id;

  /*printf("Would insert filters here\n");
show_filter(*filter);
*/

  for (i = 0; i < 2; i++) {
    if (i == 0)
      side = sendSide;
    else
      side = recvSide;

    if ((rc = InsertFilter(cid, i, which, filter)) < 0) {
      PrintError("Couldn't insert filter", rc);
      return -1;
    }
    id = filter->id;
    maxFilterID[i] = (id > maxFilterID[i]) ? id : maxFilterID[i];

  }

  return 0;
}

/* Compares all the input filters against the ip address associated with
   a target, and places all matching filters in the output list */
int match_filters(FailFilter *input, int insize, target_t match,
		 FailFilter **output, int *outsize)
{
  int i, j, ip1, ip2, ip3, ip4;

  if (target_to_ip(match, &ip1, &ip2, &ip3, &ip4))
    return -1;

  if ((*output = (FailFilter *)malloc(insize * sizeof(FailFilter))) == NULL)
    return -1;

  *outsize = 0;
  
  for (i = 0; i < insize; i++) {
    if ((input[i].ip1 == ip1) && (input[i].ip2 == ip2) &&
	(input[i].ip3 == ip3) && (input[i].ip4 == ip4)) {
      bcopy(&input[i], output[*outsize], sizeof(FailFilter));
      *outsize = *outsize + 1;
    }
  }

  return 0;
}

/* Removes the filter that matches the one specified */
void remove_filter(FailFilter filter)
{
  int rc;

  if (rc = RemoveFilter(cid, recvSide, filter.id))
    if (rc = RemoveFilter(cid, sendSide, filter.id))
      PrintError("Couldn't remove filter", rc);
}

/* Removes all filters on the current target */
int clear_filters()
{
  int i, rc, side;

  for (i = 0; i < 2; i++) {
      if (i == 0)
	side = sendSide;
      else
	side = recvSide;

      if (rc = PurgeFilters(cid, side)) {
        PrintError("Couldn't clear filters", rc);
        return 1;
      }
  }
  return 0;
}

/* Lists all filters installed on the current target */
int list_filters(FailFilter **filters, int *num_filters)
{
    RPC2_BoundedBS filtersBS;
    FailFilterSide side;
    int i, j, rc, size;

    size = (maxFilterID[recvSide] + maxFilterID[sendSide]) * sizeof(FailFilter);

    *num_filters = 0;

    if (size == 0)
      return 0;

    *filters = (FailFilter *)malloc(size);

    filtersBS.MaxSeqLen = size / 2;
    filtersBS.SeqLen = 1;

    for (i = 0; i < 2; i++) {
      if (i == 0) {
	side = sendSide;
	filtersBS.SeqBody = (RPC2_ByteSeq) *filters;
      } else {
	side = recvSide;
	filtersBS.SeqBody = (RPC2_ByteSeq) (*filters + *num_filters);
      }

      if (rc = GetFilters(cid, side, &filtersBS)) {
	PrintError("Couldn't list filters", rc);
	free(*filters);
	return -1;
      }

      if ((rc = CountFilters(cid, side)) < 0) {
	PrintError("Couldn't count filters", rc);
	free(*filters);
	return -1;
      }

      *num_filters = *num_filters + rc;
    }

    for (j = 0; j < *num_filters; j++)
      ntohFF(*filters + j);

    return 0;
}

/* Displays the contents of a filter */
int show_filter(FailFilter filter)
{
    unsigned char hostaddr[4];
    struct hostent *he;
    char buf[MAXHOSTNAMELEN + 256];

    hostaddr[0] = (unsigned char)filter.ip1;
    hostaddr[1] = (unsigned char)filter.ip2;
    hostaddr[2] = (unsigned char)filter.ip3;
    hostaddr[3] = (unsigned char)filter.ip4;

    if ((he = gethostbyaddr(hostaddr, 4, AF_INET)) != NULL)
      sprintf(buf, "%s", he->h_name);
    else
      sprintf(buf, "%d.%d.%d.%d", filter.ip1, filter.ip2, filter.ip3,
	      filter.ip4);

    printf("%2d: host %s color %d len %d-%d prob %d speed %d\n", filter.id,
	   buf, filter.color, filter.lenmin, filter.lenmax, filter.factor,
	   filter.speed);
    return 0;
}



/***** Target structures / functions *****/

/* Internal to filtutil.c, used for argument processing */
typedef enum {
  H_UNKNOWN = 0,
  H_CLIENT = 1,
  H_SERVER = 2
} host_type;

/* Parses the input and builds a list of targets */
void get_targets(int argc, char **argv, target_t **list, int *num_targets)
{
  int i;
  host_type type = H_UNKNOWN;
  target_t *targets;

  *num_targets = 0;
  *list = (target_t *)malloc(argc * sizeof(target_t));
  targets = *list;

  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-')
      switch (argv[i][1]) {
      case 'c':
	type = H_CLIENT;
	break;
      case 's':
       	type = H_SERVER;
	break;
      default:
	PrintError("Must specify client (-c) or server (-s)", 0);
	return;
	break;
      }
    else {
      strncpy(targets[*num_targets].hostname, argv[i], MAXHOSTNAMELEN);

      if (type == H_UNKNOWN) {
	PrintError("Must specify client (-c) or server (-s)", 0);
	*num_targets = 0;
	return;
      }

      targets[*num_targets].server = (type == H_SERVER);

      (*num_targets)++;
    }
  }
}

/* Special case of get_targets for commands that expect only a pair of
   targets */
int get_targ_pair(int argc, char **argv, target_t *target1, target_t *target2)
{
  int num_targets;
  target_t *targets;

  get_targets(argc, argv, &targets, &num_targets);

  if (num_targets == 2) {
    bcopy(&targets[0], target1, sizeof(target_t));
    bcopy(&targets[1], target2, sizeof(target_t));
    return 0;
  } else {
    printf("%s only works with two hosts.\n", argv[0]);
    return -1;
  }
}

/* Converts a target's hostname into an IP address */
int target_to_ip(target_t target, int *ip1, int *ip2, int *ip3, int *ip4)
{
  struct hostent *host;

  host = gethostbyname(target.hostname);

  if (host != NULL) {
    *ip1 = ((unsigned char *)host->h_addr)[0];
    *ip2 = ((unsigned char *)host->h_addr)[1];
    *ip3 = ((unsigned char *)host->h_addr)[2];
    *ip4 = ((unsigned char *)host->h_addr)[3];
  } else 
    if ((sscanf(target.hostname, "%d.%d.%d.%d", ip1, ip2, ip3, ip4) != 4) ||
	*ip1 < -1 || *ip1 > 255 ||
	*ip2 < -1 || *ip2 > 255 ||
	*ip3 < -1 || *ip3 > 255 ||
	*ip4 < -1 || *ip4 > 255) {
      printf("No such host as %s.\n", target.hostname);
      return -1;
    }

  return 0;
}



/***** RPC2 variables / functions *****/

/* Initialize RPC for use by programs */
void InitRPC()
{
  PROCESS mylpid;
  int rc;

  CODA_ASSERT(LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, &mylpid) == LWP_SUCCESS);

  rc = RPC2_Init(RPC2_VERSION, 0, NULL,  -1, NULL);
  if (rc == RPC2_SUCCESS) return;
  PrintError("InitRPC", rc);
  if (rc < RPC2_ELIMIT) exit(-1);
}

/* Bind the RPC2 socket to the target host */
int open_connection(target_t target)
{
    int rc;
    static RPC2_HostIdent hident;
    static RPC2_PortIdent pident;
    static RPC2_SubsysIdent sident;
    static RPC2_BindParms bparms;

    printf("Trying to bind to %s...", target.hostname);

    hident.Tag = RPC2_HOSTBYNAME;
    strncpy(hident.Value.Name, target.hostname, MAXHOSTNAMELEN);
    
    sident.Value.SubsysId = FCONSUBSYSID;
    sident.Tag = RPC2_SUBSYSBYID;

    pident.Tag = RPC2_PORTBYINETNUMBER;
    pident.Value.InetPortNumber = htons(target.server ? PORT_codasrv : PORT_venus);

    bparms.SecurityLevel = RPC2_OPENKIMONO;
    bparms.SharedSecret = NULL;
    bparms.ClientIdent = NULL;
    bparms.SideEffectType = 0;
    bparms.Color = FAIL_IMMUNECOLOR;

    rc = RPC2_NewBinding(&hident, &pident, &sident, &bparms, &cid);

    if (rc != RPC2_SUCCESS) {
        PrintError("Can't bind", rc);
        return -1;
    }
    RPC2_SetColor(cid, FAIL_IMMUNECOLOR);

    printf("Succeeded.\n");

    maxFilterID[recvSide] = CountFilters(cid, recvSide);
    if (maxFilterID[recvSide] < 0) {
      PrintError("Couldn't count filters", maxFilterID[recvSide]);
      return -1;
    }
    maxFilterID[sendSide] = CountFilters(cid, sendSide);
    if (maxFilterID[sendSide] < 0) {
      PrintError("Couldn't count filters", maxFilterID[sendSide]);
      return -1;
    }

    return 0;
}

/* Unbind the RPC2 connection */
void close_connection()
{
  int rc;

  if ((rc = RPC2_Unbind(cid)))
    PrintError("Could not unbind", rc);

  return;
}

/* Handle RPC2 / errno error messages */
void PrintError(char *msg, int err)
{
    extern int errno;
    
    if (err == 0) perror(msg);
    else printf("%s: %s\n", msg, RPC2_ErrorMsg(err));
}
