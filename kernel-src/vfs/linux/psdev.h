#ifndef __CODA_PSDEV_H
#define __CODA_PSDEV_H

#define CODA_PSDEV_MAJOR 67      /* Major 18 is reserved for networking   */
#define MAX_CODADEVS  5	      /* How many devices  	*/
/* #define MAX_QBYTES    32768   * Maximum bytes in the queue * */

#include <linux/config.h>
struct vcomm {
	u_long		vc_seq;
	struct wait_queue * 		vc_selproc;
	struct queue	vc_requests;
	struct queue	vc_replies;
};

#define	VC_OPEN(vcp)	    ((vcp)->vc_requests.forw != NULL)
#define MARK_VC_CLOSED(vcp) (vcp)->vc_requests.forw = NULL;

extern int cfsnc_use;

struct vmsg {
    struct queue vm_chain;
    caddr_t	 vm_data;
    u_short	 vm_flags;
    u_short      vm_inSize;	/* Size is at most 5000 bytes */
    u_short	 vm_outSize;
    u_short	 vm_opcode; 	/* copied from data to save ptr lookup */
    int		 vm_unique;
    struct wait_queue *	 vm_sleep;	/* Not used by Mach. */
    unsigned long vm_posttime;
};



extern void coda_psdev_detach(int unit);
extern int  init_coda_psdev(void);

#endif
