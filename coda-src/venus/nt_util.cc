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

#ifdef __CYGWIN32__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <winbase.h>

#include "coda_string.h"
#include "coda.h"
#include "util.h"

#ifdef __cplusplus
}
#endif

#include "nt_util.h"

/*
 * NT specific routines 
 *
 */

// Mounts ...

// Parameters for nd_do_mounts ... both for direct call
// and via CreateThread.
static char drive;
static int  mount;

int
wcslen (PWCHAR wstr)
{
    int len = 0;
    while (*wstr++) len++;
    return len;
}


// Can't use "normal" parameters due ot the fact that this will 
// be called to start a new thread.  Use the static it for mount
// communication.
static DWORD
nt_do_mounts (void *junk)
{
    HANDLE h;
    OW_PSEUDO_MOUNT_INFO info;
    DWORD nBytesReturned;
    CHAR outBuf[4096];
    DWORD d;
    int n;
    int ctlcode = OW_FSCTL_DISMOUNT_PSEUDO;
    
    WCHAR link[20] = L"\\??\\X:";  
    
    // Parameters ...
    link[4] = (short)drive;
    if (mount)
	ctlcode = OW_FSCTL_MOUNT_PSEUDO;
    
    d = DefineDosDevice(DDD_RAW_TARGET_PATH, "codadev", "\\Device\\codadev");
    if ( d == 0 ) {
	if (mount) {
	    eprint ("DDD failed, mount failed.  Killing venus.");
	    kill(getpid(), SIGKILL);
	    exit(1);
	} else {
	    eprint ("DDD failed, umount failed.");
	    return 1;
	}
    }

    h = CreateFile ("\\\\.\\codadev", GENERIC_READ | GENERIC_WRITE,
		    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
		    OPEN_EXISTING, 0, NULL);

    if (h == INVALID_HANDLE_VALUE) {
	if (mount) {
	    eprint ("CreateFile failed, mount failed.  Killing venus.");
	    kill(getpid(), SIGKILL);
	    exit(1); 
	} else { 
	    eprint ("CreateFile failed, umount failed.");
	    return 1;
	}
    } 
    
    // Set up the info for the DeviceIoControl.
    info.PseudoVolumeHandle = (HANDLE*)1;
    info.PseudoDeviceName = L"\\Device\\coda";
    info.PseudoDeviceNameLength =
	wcslen(info.PseudoDeviceName) * sizeof(WCHAR);
    info.PseudoLinkName = link;
    info.PseudoLinkNameLength = wcslen(info.PseudoLinkName) * sizeof(WCHAR);
    
    d = DeviceIoControl(h, ctlcode, &info, sizeof(info), NULL, 0,
			&nBytesReturned, NULL);
    if (!d) {
	if (mount) {
	    eprint ("Mount failed.  Killing venus.  (Error %d)",
		    GetLastError());
	    kill(getpid(), SIGKILL);
	    exit(1);
	} else {
	    eprint ("Umount failed. (Not a problem on startup.)");
	    return 1;
	}
    } 

    return 0;
}

void nt_umount (char *drivename)
{
    drive = drivename[0];
    mount = 0;
    (void) nt_do_mounts (NULL);
}

void
nt_mount (char *drivename)
{
    HANDLE h;

    nt_umount (drivename);

    mount = 1;
    
    h = CreateThread(0, 0, nt_do_mounts, NULL, 0, NULL);

    if (!h) {
	eprint ("CreateThread failed.  Mount unsuccessful.  Killing venus.");
	kill(getpid(), SIGKILL);
	exit(1); 
    }

    CloseHandle (h);
}

//
// kernel -> venus ... using a socket pair.
//

static int sockfd;
static volatile int doexit;

static DWORD
listen_kernel (void *junk)
{
    HANDLE h;
    int rc;
    DWORD bytesret;
    char outbuf[VC_MAXMSGSIZE];

    // Get the device ready;
    rc = DefineDosDevice(DDD_RAW_TARGET_PATH, "codadev", "\\Device\\codadev");
    if ( rc == 0 ) {
	eprint ("DDD failed, listen_kernel failed.");
	kill(getpid(), SIGKILL);
	exit(1); 
    }

    h = CreateFile ("\\\\.\\codadev", GENERIC_READ | GENERIC_WRITE,
		    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
		    OPEN_EXISTING, 0, NULL);

    if (h == INVALID_HANDLE_VALUE) {
	eprint ("CreateFile failed, listen_kernel failed.");
	kill(getpid(), SIGKILL);
	exit(1); 
    } 

    while (1) {
	// Do a device ioctl.
	bytesret = 0;
	rc = DeviceIoControl (h, CODA_FSCTL_FETCH, NULL, 0, outbuf,
			      VC_MAXMSGSIZE, &bytesret, NULL);
	if (rc) {
	    if (bytesret > 0) {
		write (sockfd, (char *)&bytesret, sizeof(bytesret));
		write (sockfd, outbuf, bytesret);
	    }
	} else {
	    eprint ("listen_kernel: fetch failed");
	}
	if (doexit)
	  ExitThread(0);
    }
}

// "public" interface for ipc

static HANDLE kerndev;
static HANDLE kernelmon;

int nt_initialize_ipc (int sock)
{
    int rc;

    // Get the device ready for writing to kernel
    rc = DefineDosDevice(DDD_RAW_TARGET_PATH, "codadev", "\\Device\\codadev");
    if ( rc == 0 ) {
	eprint ("nt_initialize_ipc: DDD failed.");
	return 0; 
    }

    kerndev = CreateFile ("\\\\.\\codadev", GENERIC_READ | GENERIC_WRITE,
			  FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
			  OPEN_EXISTING, 0, NULL);

    if (kerndev == INVALID_HANDLE_VALUE) {
	eprint ("nt_initialize_ipc: CreateFile failed. "
		"Is the coda service started?");
	return 0; 
    } 

    // Other initialization
    sockfd = sock;
    doexit = 0;

    // Start the kernel monitor
    kernelmon = CreateThread (0, 0, listen_kernel, NULL, 0, NULL);
    if (kernelmon == NULL) {
	return 0;
    }

    // All was successful 
    return 1;
}

int nt_msg_write (char *buf, int size)
{
    int rc;
    DWORD bytesret;

    //    eprint ("nt_msg_write: Start\n");
    rc = DeviceIoControl (kerndev, CODA_FSCTL_ANSWER, buf, size, NULL, 0,
			  &bytesret, NULL);
    //    eprint ("nt_msg_write: End\n");
    if (!rc)
	return 0;
    
    return size;
}

void nt_stop_ipc (void)
{
    (void) TerminateThread (kernelmon,  0);
    CloseHandle(kernelmon);
    doexit = 1;
}



//
// NT service routines ... to get venus to run nicely and between
// logins, it must run as a service ... Thus all this goo....
//

// The main program for venus changes names ....

int venus_main (int, char**);

// "global" State ...

static SERVICE_STATUS          mystat;
static SERVICE_STATUS_HANDLE   myhand;

// Service defines ......

#define ServiceName "venus"

// Service prototypes

static void ntsrv_start (DWORD argc, LPTSTR *argv);
static void ntsrv_install (void);
static void ntsrv_remove (void);
static void ntsrv_ctrl (DWORD op);



// This is the main program for running under the service manager
int
main (int argc, char **argv)
{
    SERVICE_TABLE_ENTRY Dispatch[] = {
	{ TEXT(ServiceName), ntsrv_start },
	{ NULL, NULL }
    };
    
    if (argc == 2 && strcmp("-install", argv[1]) == 0) {
	ntsrv_install();
	return 0;
    }
    
    if (argc == 2 && strcmp("-remove", argv[1]) == 0) {
	ntsrv_remove();
	return 0;
    }

    if (!StartServiceCtrlDispatcher (Dispatch)) {
	return venus_main (argc, argv);
    }
    
    return 0;
}


// Service Control Handler Function

void ntsrv_ctrl (DWORD op)
{
    if (op == SERVICE_CONTROL_STOP || op == SERVICE_CONTROL_SHUTDOWN) {
	
	// Say we are stopping ...
	
	mystat.dwCurrentState  = SERVICE_STOP_PENDING;
	mystat.dwWin32ExitCode = 0;
	mystat.dwCheckPoint    = 1;
	mystat.dwWaitHint      = 2000;
	
	if (SetServiceStatus (myhand, &mystat)) {
	    eprint ("ntsrv_ctrl: SetServiceStatus (1) error %d\n",
		     GetLastError());
	}

	// Now kill ourselves!  and return (main tell about final state?)
	kill (getpid(), SIGTERM);
	return;
    }
    
    (void) SetServiceStatus (myhand, &mystat);
    return;
}


void ntsrv_start (DWORD argc, LPTSTR *argv)
{
    myhand = RegisterServiceCtrlHandler (TEXT(ServiceName),  ntsrv_ctrl);
    
    if (!myhand) {
	eprint ("srv_start: Could not register Ctrl Handler (%d)\n",
		GetLastError());
	return;
    }

    // Next, say we are running ....
    
    mystat.dwServiceType      = SERVICE_WIN32_OWN_PROCESS;
    mystat.dwCurrentState     = SERVICE_RUNNING;
    mystat.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    mystat.dwWin32ExitCode    = 0;
    mystat.dwServiceSpecificExitCode = 0;
    mystat.dwCheckPoint       = 0;
    mystat.dwWaitHint         = 0;

    if (SetServiceStatus (myhand, &mystat)) {
	eprint ("srv_start: SetServiceStatus (2) error %d\n",
		GetLastError());
    }

    // Call the real program main!
    
    venus_main (argc, argv);
    
    // Now say we are stopped ...
    
    mystat.dwCurrentState  = SERVICE_STOPPED;
    mystat.dwWin32ExitCode = 0;
    mystat.dwCheckPoint    = 0;
    mystat.dwWaitHint      = 0;
    
    if (SetServiceStatus (myhand, &mystat)) {
	eprint ("srv_start: SetServiceStatus (4) error %d\n",
		GetLastError());
    }
    
    return;
    
}


void ntsrv_install (void)
{
    char name[MAXPATHLEN];
    
    SC_HANDLE SCMan;
    SC_HANDLE Srv;
    
    if (GetModuleFileName (NULL, name, MAXPATHLEN) == 0) {
	printf ("Could not get the program's file name!  Aborting.\n");
	return;
    }
    
    SCMan = OpenSCManager (NULL, NULL, SC_MANAGER_ALL_ACCESS);
    
    if (!SCMan) {
	printf ("Could not talk to the service manager. Aborting.\n");
	return;
    }
    
    Srv = CreateService (SCMan, TEXT(ServiceName), TEXT(ServiceName), 
			 SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, 
			 SERVICE_DEMAND_START,  SERVICE_ERROR_NORMAL, 
			 name, NULL,  NULL, TEXT("\0\0"), NULL, NULL);
    
    if (Srv) {
	printf ("'%s' service installed.\n", ServiceName);
	CloseServiceHandle(Srv);
    } else {
	printf ("'%s' service not installed.\n", ServiceName);
    }
    
    CloseServiceHandle(SCMan);
} 


void ntsrv_remove (void) 
{
    SC_HANDLE       SCMan;
    SC_HANDLE       Srv;
    SERVICE_STATUS  SrvStat;
    
    SCMan = OpenSCManager (NULL, NULL, SC_MANAGER_ALL_ACCESS);
    
    if (!SCMan) {
	printf ("Could not talk to the service manager. Aborting.\n");
	return;
    }
    
    Srv = OpenService(SCMan, TEXT(ServiceName), SERVICE_ALL_ACCESS);
    
    if (!Srv) {
        printf ("No such service: '%s'\n", ServiceName);
	CloseServiceHandle (SCMan);
	return;
    }
    
    // Is it stopped?
    if (QueryServiceStatus (Srv, &SrvStat) ) {
        if (SrvStat.dwCurrentState != SERVICE_STOPPED) {
	    printf ("Please stop the service first.\n");
	    CloseServiceHandle (Srv);
	    CloseServiceHandle (SCMan);
	    return;
	}
    }
    
    // Now delete it.
    if (DeleteService (Srv))
	printf ("Service Deleted.\n");
    else
	printf ("Service was not deleted:  error number %d\n", GetLastError());
    
    CloseServiceHandle (Srv);
    CloseServiceHandle (SCMan);
}


#endif
