/*
 *	This seems the best way to handle this. sys/socket.h already has
 *	all the right bits in it. In fact there isn't a single useful thing
 *	in the BSD net-2 sys/socketvar.h anyway but people persist in including
 *	it...
 *		Alan
 */
#include <sys/socket.h>
