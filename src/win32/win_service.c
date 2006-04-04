/*    $OSSEC, service.c, v0.1, 2006/04/03, Daniel B. Cid$    */

/* Copyright (C) 2006 Daniel B. Cid <dcid@ossec.net>
 * All rights reserved.
 *
 * This program is a free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation
 */


#ifdef WIN32

#include "shared.h"

#ifndef ARGV0
#define ARGV0 ossec-agent
#endif

static LPTSTR g_lpszServiceName        = "OssecSvc";
static LPTSTR g_lpszServiceDisplayName = "OSSEC Hids";
static LPTSTR g_lpszServiceDescription = "OSSEC Hids Windows Agent";

static LPTSTR g_lpszRegistryKey        = "SOFTWARE\\Ossec";
static LPTSTR g_lpszRegistryCmdFormat  = "CmdLineParam_%03d";
static LPTSTR g_lpszRegistryCountFormat= "CmdLineParamCount";

static SERVICE_STATUS          ossecServiceStatus;
static SERVICE_STATUS_HANDLE   ossecServiceStatusHandle;

/* ServiceStart */
void WINAPI OssecServiceStart (DWORD argc, LPTSTR *argv);


                    
/* int InstallService()
 * Install the OSSEC HIDS agent service.
 */
int InstallService(int argc, char **argv) 
{
    int iArgCounter;
    long lRegRC = 0;
    char buffer[MAX_PATH+1];
    char tmp_str;

            
    SC_HANDLE schSCManager, schService;
    LPCTSTR lpszBinaryPathName = NULL;
    HKEY hkossec = NULL;
    DWORD dwWriteCounter = 0;
    SERVICE_DESCRIPTION sdBuf;
    

    /* Cleaning up some variables */
    buffer[MAX_PATH] = '\0';
    
    
    /* Executable path -- it must be called with the
     * full path
     */
    lpszBinaryPathName = argv[0]; 
 
    /* Create the registry key */ 
    /*
    lRegRC = RegCreateKeyEx(HKEY_LOCAL_MACHINE,
                            g_lpszRegistryKey,
                            0,
                            NULL,
                            REG_OPTION_NON_VOLATILE,
                            KEY_ALL_ACCESS,
                            NULL,
                            &hkossec,
                            NULL
                            );

    if( lRegRC != ERROR_SUCCESS )
    {
        goto install_error;
    }
    */                                        
    /* Closing the registry */
    /*
    lRegRC = RegCloseKey( hkossec );
    if( lRegRC != ERROR_SUCCESS )
    {
        goto install_error;
    }
    */
                                                 
    /* Opening the services database */
    schSCManager = OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS);

    if (schSCManager == NULL)
    {
        goto install_error;
    }

    /* Creating the service */
    schService = CreateService(schSCManager, 
                               g_lpszServiceName,
                               g_lpszServiceDisplayName,
                               SERVICE_ALL_ACCESS,
                               SERVICE_WIN32_OWN_PROCESS,
                               SERVICE_DEMAND_START,
                               SERVICE_ERROR_NORMAL,
                               lpszBinaryPathName,
                               NULL, NULL, NULL, NULL, NULL);
    
    if (schService == NULL)
    {
        goto install_error;
    }

    /* Setting description */
    sdBuf.lpDescription = g_lpszServiceDescription;
    if(!ChangeServiceConfig2(schService, SERVICE_CONFIG_DESCRIPTION, &sdBuf))
    {
        goto install_error;
    }
    
    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);

    printf(" [%s] Successfully added to the Services database.\n", ARGV0);
    return(1);


    install_error:
    {
        char local_msg[1025];
        LPVOID lpMsgBuf;
        TCHAR szMsg[1001];
        
        memset(szMsg, 0, 1001);
        memset(local_msg, 0, 1025);

        FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER |
                       FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL,
                       GetLastError(),
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                       (LPTSTR) &lpMsgBuf,
                       0,
                       NULL);

        snprintf(local_msg, 1024, "[%s] Unable to create registry "
                                  "entry: %s", ARGV0,(LPCTSTR)lpMsgBuf);
        fprintf(stderr, "%s", local_msg);
        CreateApplicationEventLogEntry(local_msg); 
        return(0);
    }
}


/* int UninstallService()
 * Uninstall the OSSEC HIDS agent service.
 */
int UninstallService() 
{
    SC_HANDLE schSCManager, schService;
    HKEY hkossec = NULL;
    long lRegRC = 0;

    
    /* lRegRC = RegDeleteKey( HKEY_LOCAL_MACHINE, g_lpszRegistryKey); */

    /* Removing from the services database */
    schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (schSCManager)
    {
        schService = OpenService(schSCManager,g_lpszServiceName,DELETE);
        if(schService)
        {
            if (DeleteService(schService))

            {
                CloseServiceHandle(schService);
                CloseServiceHandle(schSCManager);

                printf(" [%s] Successfully removed from "
                       "the Services database.\n", ARGV0);
                return(1);
            }
            CloseServiceHandle(schService);
        }
        CloseServiceHandle(schSCManager);
    }

    fprintf(stderr, " [%s] Error removing from "
                    "the Services database.\n", ARGV0);
    
    return(0);
}


/** VOID WINAPI OssecServiceCtrlHandler (DWORD dwOpcode)
 * "Signal" handler
 */
VOID WINAPI OssecServiceCtrlHandler (DWORD dwOpcode)
{
    switch(dwOpcode)
    {
        case SERVICE_CONTROL_STOP:
            ossecServiceStatus.dwCurrentState           = SERVICE_STOPPED;
            ossecServiceStatus.dwWin32ExitCode          = 0;
            ossecServiceStatus.dwCheckPoint             = 0;
            osecServiceStatus.dwWaitHint                = 0;

            SetServiceStatus (ossecServiceStatusHandle, &ossecServiceStatus);
            return;
        default:
            break;
    }
    return;
}
 

 
/** int os_WinMain(int argc, char **argv)
 * Initializes OSSEC dispatcher
 */
int os_WinMain(int argc, char **argv) 
{
    SERVICE_TABLE_ENTRY   steDispatchTable[] =
    {
        { g_lpszServiceName, OssecServiceStart },
        { NULL,       NULL                     }
    };

    /
    if(!StartServiceCtrlDispatcher(steDispatchTable))
    {
        merror("%s: Unable to start.", ARGV0);
        return(0);
    }

    return(1);
}


/** void WINAPI OssecServiceStart (DWORD argc, LPTSTR *argv)
 * Starts OSSEC service
 */
void WINAPI OssecServiceStart (DWORD argc, LPTSTR *argv)
{
    int i;
    int iArgCounter;
    char** argvDynamic = NULL;
    char errorbuf[PCAP_ERRBUF_SIZE];
    char *interfacenames = NULL;

    DWORD dwStatus;
    DWORD dwSpecificError;

    ossecServiceStatus.dwServiceType            = SERVICE_WIN32;
    ossecServiceStatus.dwCurrentState           = SERVICE_START_PENDING;
    ossecServiceStatus.dwControlsAccepted       = SERVICE_ACCEPT_STOP;
    ossecServiceStatus.dwWin32ExitCode          = 0;
    ossecServiceStatus.dwServiceSpecificExitCode= 0;
    ossecServiceStatus.dwCheckPoint             = 0;
    osecServiceStatus.dwWaitHint                = 0;

    ossecServiceStatusHandle = 
        RegisterServiceCtrlHandler(g_lpszServiceName, 
                                   OssecServiceCtrlHandler);

    if (ossecServiceStatusHandle == (SERVICE_STATUS_HANDLE)0)
    {
        merror("%s: RegisterServiceCtrlHandler failed", ARGV0);
        return;
    }

    ossecServiceStatus.dwCurrentState = SERVICE_RUNNING;
    ossecServiceStatus.dwCheckPoint = 0;
    osecServiceStatus.dwWaitHint = 0;

    if (!SetServiceStatus(ossecServiceStatusHandle, &ossecServiceStatus))
    {
        merror("%s: SetServiceStatus error", ARGV0);
        return;
    }
                                                            
}


#endif
/* EOF */
