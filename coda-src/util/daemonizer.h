/* BLURB gpl

                           Coda File System
                              Release 6

             Copyright (c) 2004 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _DAEMONIZER_H_
#define _DAEMONIZER_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Fork into the background and if pidfile != NULL write our process id
 * to that file. this function returns an fd, which we use to signal the
 * parent whenever we're up and running.
 * stdout and stderr still have to be redirected */
int daemonize(void);

/* write our pid to pidfile and keep the file locked */
void update_pidfile(const char *pidfile);

/* Let the parent process know that we've succesfully started. */
void gogogo(int parent_fd);

#ifdef __cplusplus
}
#endif

#endif /* _DAEMONIZER_H_ */
