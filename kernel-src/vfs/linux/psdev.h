#ifndef __CODA_PSDEV_H
#define __CODA_PSDEV_H

#define CODA_PSDEV_MAJOR 67      /* Major 18 is reserved for networking   */
#define MAX_CODADEVS  5	      /* How many devices  	*/
/* #define MAX_QBYTES    32768   * Maximum bytes in the queue * */

#include <linux/config.h>


extern void coda_psdev_detach(int unit);
extern int  init_coda_psdev(void);

#endif
