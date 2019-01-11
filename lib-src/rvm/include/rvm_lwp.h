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

/* LWP compatability for RVM */

#include <lwp/lwp.h>
#include <lwp/lock.h>

#define RVM_STACKSIZE 1024 * 16
#define BOGUSCODE (BOGUS_USE_OF_CTHREADS) /* force compilation error */

#define RVM_MUTEX struct Lock
#define RVM_CONDITION char
#define MUTEX_INITIALIZER \
    {                     \
        0, 0, 0, 0        \
    }

/* Supported cthread definitions */

#define cthread_t PROCESS
static inline PROCESS cthread_fork(void (*fname)(void *), void *arg)
{
    PROCESS rvm_lwppid;
    LWP_CreateProcess(fname, RVM_STACKSIZE, LWP_NORMAL_PRIORITY, arg,
                      "rvm_thread", &rvm_lwppid);
    return rvm_lwppid;
}
#define cthread_init()                                    \
    do {                                                  \
        LWP_Init(LWP_VERSION, LWP_NORMAL_PRIORITY, NULL); \
        IOMGR_Initialize();                               \
    } while (0)
#define cthread_exit(retval) return
#define cthread_yield()        \
    do {                       \
        IOMGR_Poll();          \
        LWP_DispatchProcess(); \
    } while (0)
#define cthread_join(thread) (0)

#define condition_wait(c, m)   \
    do {                       \
        ReleaseWriteLock((m)); \
        LWP_WaitProcess((c));  \
        ObtainWriteLock((m));  \
    } while (0)
#define condition_signal(c) (LWP_SignalProcess((c)))
#define condition_broadcast(c) (LWP_SignalProcess((c)))
#define condition_clear(c) /* nop  */
#define condition_init(c) /* nop */
#define mutex_init(m) (Lock_Init(m))
#define mutex_clear(m) /* nop */
#define LOCK_FREE(m) (!WriteLocked(&(m)))

static inline PROCESS cthread_self(void)
{
    PROCESS rvm_lwppid;
    LWP_CurrentProcess(&rvm_lwppid);
    return rvm_lwppid;
}

/* synchronization tracing definitions of lock/unlock */
#ifdef DEBUGRVM
#define mutex_lock(m)                                                   \
    do {                                                                \
        printf("mutex_lock OL(0x%x)%s:%d...", (m), __FILE__, __LINE__); \
        ObtainWriteLock((m));                                           \
        printf("done\n");                                               \
    } while (0)
#define mutex_unlock(m)                                                   \
    do {                                                                  \
        printf("mutex_unlock RL(0x%x)%s:%d...", (m), __FILE__, __LINE__); \
        ReleaseWriteLock((m));                                            \
        printf("done\n");                                                 \
    } while (0)
#else /* !DEBUGRVM */
#define mutex_lock(m) ObtainWriteLock((m))
#define mutex_unlock(m) ReleaseWriteLock((m))
#endif /* !DEBUGRVM */

/* Unsupported cthread calls */

#define mutex_alloc() BOGUSCODE
#define mutex_set_name(m, x) BOGUSCODE
#define mutex_name(m) BOGUSCODE
#define mutex_free(m) BOGUSCODE

#define condition_alloc() BOGUSCODE
#define condition_set_name(c, x) BOGUSCODE
#define condition_name(c) BOGUSCODE
#define condition_free(c) BOGUSCODE

#define cthread_detach() BOGUSCODE
#define cthread_sp() BOGUSCODE
#define cthread_assoc(id, t) BOGUSCODE
#define cthread_set_name BOGUSCODE
#define cthread_name BOGUSCODE
#define cthread_count() BOGUSCODE
#define cthread_set_limit BOGUSCODE
#define cthread_limit() BOGUSCODE
#define cthread_set_data(t, x) BOGUSCODE
#define cthread_data(t) BOGUSCODE
