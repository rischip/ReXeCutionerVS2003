///////////////////////////////////////////////////////////////////////////////////
//
// Author: Zoltan Csizmadia, 3/2001
//
// This code is freeware! If you change it, or want to use it, 
// please send an email!
//
// Email: zoltan_csizmadia@yahoo.com
//
// For companies(Austin,TX): If you would like to get my resume, send an email!
//
///////////////////////////////////////////////////////////////////////////////////
//
// Service.cpp
//
// History:
//
//    3/27/2001      Initial version
//
///////////////////////////////////////////////////////////////////////////////////
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <winsvc.h>
#include <process.h>
#include "ReXeCutionerSvc.h"
#include "../ReXeCutioner.h"
#include <winbase.h>

SERVICE_STATUS          ServiceStatus; 
SERVICE_STATUS_HANDLE   ServiceStatusHandle; 
 
HANDLE hStopServiceEvent = NULL;

VOID  WINAPI ReXeCutionerStart (DWORD argc, LPTSTR *argv); // prototype for the starting point of the service
VOID  WINAPI ReXeCutionerCtrlHandler (DWORD opcode); // prototype for the control handler callback function of the service
DWORD IsService( BOOL& );

int _tmain( int, LPTSTR* )
{
   SERVICE_TABLE_ENTRY DispatchTable[] = { 
        { SERVICENAME,	ReXeCutionerStart }, 
        { NULL, NULL } }; 

   BOOL bService = TRUE;
   
   // This process should be a service :)
   IsService( bService );
   if ( !bService )
      _tprintf( _T("This is a service executable! Couldn't start directly.\n") );

   // Start service
   return StartServiceCtrlDispatcher( DispatchTable);
}

// Deletes service
void DeleteSvc()
{
   // Open service manager
   SC_HANDLE hSCM = ::OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS);

   if (hSCM == NULL)
      return;

   // OPen service
   SC_HANDLE hService = ::OpenService( hSCM, SERVICENAME, SERVICE_ALL_ACCESS );

   if (hService == NULL)
   {
      ::CloseServiceHandle(hSCM);
      return;
   }

   // Deletes service from service database
   DeleteService( hService );

   // Stop the service
   ServiceStatus.dwCurrentState       = SERVICE_STOPPED; 
   ServiceStatus.dwCheckPoint         = 0; 
   ServiceStatus.dwWaitHint           = 0; 
   ServiceStatus.dwWin32ExitCode      = 0; 
   ServiceStatus.dwServiceSpecificExitCode = 0; 
   SetServiceStatus (ServiceStatusHandle, &ServiceStatus); 

   ::CloseServiceHandle(hService);
   ::CloseServiceHandle(hSCM);
}

// Start service
VOID WINAPI ReXeCutionerStart (DWORD, LPTSTR* ) 
{
   DWORD status = 0; 
   DWORD specificError = 0;

   // Prepare the ServiceStatus structure that will be used for the
   // comunication with SCM(Service Control Manager).
   // If you fully under stand the members of this structure, feel
   // free to change these values :o)
   //ServiceStatus.dwServiceType        = SERVICE_INTERACTIVE_PROCESS;
   ServiceStatus.dwServiceType        = SERVICE_WIN32 | SERVICE_INTERACTIVE_PROCESS; 
   ServiceStatus.dwCurrentState       = SERVICE_START_PENDING; 
   ServiceStatus.dwControlsAccepted   = SERVICE_ACCEPT_STOP; 
   ServiceStatus.dwWin32ExitCode      = 0; 
   ServiceStatus.dwServiceSpecificExitCode = 0; 
   ServiceStatus.dwCheckPoint         = 0; 
   ServiceStatus.dwWaitHint           = 0; 
   
   // Here we register the control handler for our service.
   // We tell the SCM about a call back function that SCM will
   // call when user tries to Start, Stop or Pause your service.
   ServiceStatusHandle = RegisterServiceCtrlHandler( 
         TEXT("Service"), ReXeCutionerCtrlHandler ); 

   if (ServiceStatusHandle == (SERVICE_STATUS_HANDLE)0) 
      return; 
   
   // Handle error condition 
   if (status != NO_ERROR) 
   { 
      ServiceStatus.dwCurrentState       = SERVICE_STOPPED; 
      ServiceStatus.dwCheckPoint         = 0; 
      ServiceStatus.dwWaitHint           = 0; 
      ServiceStatus.dwWin32ExitCode      = status; 
      ServiceStatus.dwServiceSpecificExitCode = specificError; 

      SetServiceStatus (ServiceStatusHandle, &ServiceStatus); 
      return; 
   } 

   // Initialization complete - report running status. 
   ServiceStatus.dwCurrentState       = SERVICE_RUNNING; 
   ServiceStatus.dwCheckPoint         = 0; 
   ServiceStatus.dwWaitHint           = 0; 

   if (!SetServiceStatus (ServiceStatusHandle, &ServiceStatus)) 
      status = GetLastError(); 
   else
   {
      // Start the main thread

      hStopServiceEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
      _beginthread( _ServiceMain, 0, NULL );
   }

   return; 
} 
 
// Service Ctrl handler
VOID WINAPI ReXeCutionerCtrlHandler (DWORD Opcode) 
{ 
    DWORD status; 
	
    switch(Opcode) 
    { 
        case SERVICE_CONTROL_STOP: 
            // Signal the event to stop the main thread
            SetEvent( hStopServiceEvent );

            ServiceStatus.dwWin32ExitCode = 0; 
            ServiceStatus.dwCurrentState  = SERVICE_STOPPED; 
            ServiceStatus.dwCheckPoint    = 0; 
            ServiceStatus.dwWaitHint      = 0; 
 
            if (!SetServiceStatus (ServiceStatusHandle, 
                &ServiceStatus))
            { 
                status = GetLastError(); 
            } 
			return; 
 
        case SERVICE_CONTROL_INTERROGATE: 
        // Fall through to send current status. 
            break; 
    } 
 
    // Send current status. 
    if (!SetServiceStatus (ServiceStatusHandle,  &ServiceStatus)) 
    { 
        status = GetLastError(); 
    } 
    return; 
}

// This process is a service or is not ?
DWORD IsService( BOOL& isService )
{
   DWORD pID = GetCurrentProcessId(); 
	HANDLE hProcessToken = NULL;
	DWORD groupLength = 50;
	PTOKEN_GROUPS groupInfo = NULL;

	SID_IDENTIFIER_AUTHORITY siaNt = SECURITY_NT_AUTHORITY;
	PSID pInteractiveSid = NULL;
	PSID pServiceSid = NULL;

	DWORD dwRet = NO_ERROR;
    
    // reset flags
	BOOL isInteractive = FALSE;
	isService = FALSE;

	DWORD ndx;

	HANDLE hProcess = OpenProcess( PROCESS_ALL_ACCESS, FALSE, pID );

	// open the token
	if (!::OpenProcessToken( hProcess, TOKEN_QUERY, &hProcessToken) )
	{
		dwRet = ::GetLastError();
		goto closedown;
	}

	// allocate a buffer of default size
	groupInfo = (PTOKEN_GROUPS)::LocalAlloc(0, groupLength);
	if (groupInfo == NULL)
	{
		dwRet = ::GetLastError();
		goto closedown;
	}

	// try to get the info
	if (!::GetTokenInformation(hProcessToken, TokenGroups,
		groupInfo, groupLength, &groupLength))
	{
		// if buffer was too small, allocate to proper size, otherwise error
		if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER)
		{
			dwRet = ::GetLastError();
			goto closedown;
		}

		::LocalFree(groupInfo);

		groupInfo = (PTOKEN_GROUPS)::LocalAlloc(0, groupLength);
		if (groupInfo == NULL)
		{
			dwRet = ::GetLastError();
			goto closedown;
		}

		if (!GetTokenInformation(hProcessToken, TokenGroups,
			groupInfo, groupLength, &groupLength))
		{
			dwRet = ::GetLastError();
			goto closedown;
		}
	}

	// create comparison sids
	if (!AllocateAndInitializeSid(&siaNt, 1, SECURITY_INTERACTIVE_RID,
		0, 0, 0, 0, 0, 0, 0, &pInteractiveSid))
	{
		dwRet = ::GetLastError();
		goto closedown;
	}

	if (!AllocateAndInitializeSid(&siaNt, 1, SECURITY_SERVICE_RID,
		0, 0, 0, 0, 0, 0, 0, &pServiceSid))
	{
		dwRet = ::GetLastError();
		goto closedown;
	}

	// try to match sids
	for (ndx = 0; ndx < groupInfo->GroupCount ; ndx += 1)
	{
		SID_AND_ATTRIBUTES sanda = groupInfo->Groups[ndx];
		PSID pSid = sanda.Sid;

		if (::EqualSid(pSid, pInteractiveSid))
		{
			isInteractive = TRUE;
			isService = FALSE;
			break;
		}
		else if (::EqualSid(pSid, pServiceSid))
		{
			isService = TRUE;
			//isInteractive = FALSE;
			isInteractive = TRUE;
			break;
		}
	}

   if ( !( isService || isInteractive ) )
		isService = TRUE;
        
closedown:
		if ( pServiceSid )
			::FreeSid( pServiceSid );

		if ( pInteractiveSid )
			::FreeSid( pInteractiveSid );

		if ( groupInfo )
			::LocalFree( groupInfo );

		if ( hProcessToken )
			::CloseHandle( hProcessToken );

		if ( hProcess )
			::CloseHandle( hProcess );

	return dwRet;
}
