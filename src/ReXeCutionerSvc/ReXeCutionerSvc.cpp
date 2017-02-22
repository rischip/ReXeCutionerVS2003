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
// ReXeCutionerSvc.cpp
//
// History:
//
//    3/27/2001      Initial version
//	  2/21/2006		 Version 2
//		
//		Chris Pelphrey
//
//		Email: riscchip@gmail.com
//
//		Added ability to run applications visible on the remote window station
//		Added ability to login using the credentials supplied on the command line
//		Changed name to ReXeCutioner
//		XOR'd sensitive portions of the comm's data structure to prevent login credentials from 
//		being passed across the network in plain text
//		
//
///////////////////////////////////////////////////////////////////////////////////
//#define UNICODE
//#define _WIN32_WINNT 0x0501
//#include <windows.h>
//#include <stdio.h>

//#define _WIN32_WINNT 0x0500 
#include <windows.h>
//#include <userenv.h>
#include <winbase.h>
#include <stdio.h>
#include <sddl.h>
#include <aclapi.h>
#include <mq.h>

#include <tchar.h>
//#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <winsvc.h>
#include <process.h>
//#include <atlsecurity.h>
#include "ReXeCutionerSvc.h"
#include "../ReXeCutioner.h"



#include <tchar.h>
#include <lmcons.h>
#include <stdio.h>
#include <io.h>
#include <direct.h>
#include <string.h>

PSID pSid = NULL;
int GUI = 0;

void CommunicationPoolThread(PVOID);
void CommunicationPipeThreadProc(PVOID);
DWORD Execute(ReXeCutionerMessage*, DWORD*);
BOOL AddAceToWindowStation(HWINSTA hwinsta, PSID psid, int remove);
BOOL AddAceToDesktop(HDESK hdesk, PSID psid, int remove);
BOOL GetLogonSID (HANDLE hToken, PSID *ppsid);
VOID FreeLogonSID (PSID *ppsid);
void DeleteMatchingAces( ACL* pdacl, void* psid );


LONG  dwSvcPipeInstanceCount = 0;
TCHAR szStdOutPipe[_MAX_PATH] = _T("");
TCHAR szStdInPipe[_MAX_PATH] = _T("");
TCHAR szStdErrPipe[_MAX_PATH] = _T("");


#define DESKTOP_ALL (DESKTOP_READOBJECTS | DESKTOP_CREATEWINDOW | \
DESKTOP_CREATEMENU | DESKTOP_HOOKCONTROL | DESKTOP_JOURNALRECORD | \
DESKTOP_JOURNALPLAYBACK | DESKTOP_ENUMERATE | DESKTOP_WRITEOBJECTS | \
DESKTOP_SWITCHDESKTOP | STANDARD_RIGHTS_REQUIRED)

#define WINSTA_ALL (WINSTA_ENUMDESKTOPS | WINSTA_READATTRIBUTES | \
WINSTA_ACCESSCLIPBOARD | WINSTA_CREATEDESKTOP | WINSTA_WRITEATTRIBUTES | \
WINSTA_ACCESSGLOBALATOMS | WINSTA_EXITWINDOWS | WINSTA_ENUMERATE | \
WINSTA_READSCREEN | STANDARD_RIGHTS_REQUIRED)

#define GENERIC_ACCESS (GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE | \
GENERIC_ALL)

#define ADD 0
#define REMOVE 1



// Service "main" function
void _ServiceMain( void* )
{
   // Start CommunicationPoolThread, which handles the incoming instances
   _beginthread( CommunicationPoolThread, 0, NULL );

   // Waiting for stop the service
   while( WaitForSingleObject( hStopServiceEvent, 10 ) != WAIT_OBJECT_0 )
   {
   }
   
   // Let's delete itself, after the service stopped
   DeleteSvc();

   CloseHandle( hStopServiceEvent );
}

// Communicaton Thread Pool, handles the incoming ReXeCutioner.exe requests
void CommunicationPoolThread(PVOID)
{
    HANDLE hPipe = NULL;
    
    for (;;)
    {
        SECURITY_ATTRIBUTES SecAttrib = {0};
        SECURITY_DESCRIPTOR SecDesc;
        InitializeSecurityDescriptor(&SecDesc, SECURITY_DESCRIPTOR_REVISION);
        SetSecurityDescriptorDacl(&SecDesc, TRUE, NULL, TRUE);

        SecAttrib.nLength = sizeof(SECURITY_ATTRIBUTES);
        SecAttrib.lpSecurityDescriptor = &SecDesc;;
        SecAttrib.bInheritHandle = TRUE;

        // Create communication pipe
        hPipe = CreateNamedPipe(
            _T("\\\\.\\pipe\\")ReXeCutionerCOMM, 
            PIPE_ACCESS_DUPLEX, 
            PIPE_TYPE_MESSAGE | PIPE_WAIT, 
            PIPE_UNLIMITED_INSTANCES,
            0,
            0,
            (DWORD)-1,
            &SecAttrib);

        if ( hPipe != NULL )
        {
            // Waiting for client to connect to this pipe
            ConnectNamedPipe( hPipe, NULL );
            _beginthread( CommunicationPipeThreadProc, 0, (void*)hPipe);
        }
    }
}

// Handles a client
void CommunicationPipeThreadProc( void* pParam )
{
   HANDLE hPipe = (HANDLE)pParam;

   ReXeCutionerMessage msg;
   ReXeCutionerResponse response;

   DWORD dwWritten;
   DWORD dwRead;

   // Increment instance counter 
   InterlockedIncrement( &dwSvcPipeInstanceCount );

   ::ZeroMemory( &response, sizeof(response) );

   // Waiting for communication message from client
   if ( !ReadFile( hPipe, &msg, sizeof(msg), &dwRead, NULL ) || dwRead == 0 )
      goto cleanup;
	  
		//XOR the data structure to prevent username and password
		//from going across the network in plain text
		//figure out encryption routine for this later
		for (unsigned int i = 0; i < strlen(msg.szCommand); i++)
		msg.szCommand[i] = 1^msg.szCommand[i];
		for (unsigned int i = 0; i < strlen(msg.szDomain); i++)
		msg.szDomain[i] = 1^msg.szDomain[i];
		for (unsigned int i = 0; i < strlen(msg.szMachine); i++)
		msg.szMachine[i] = 1^msg.szMachine[i];
		for (unsigned int i = 0; i < strlen(msg.szPassword); i++)
		msg.szPassword[i] = 1^msg.szPassword[i];
		for (unsigned int i = 0; i < strlen(msg.szUser); i++)
		msg.szUser[i] = 1^msg.szUser[i];

		

   // Execute the requested command
   response.dwErrorCode  = Execute( &msg, &response.dwReturnCode );
   
   // Send back the response message (client is waiting for this response)
   if ( !WriteFile( hPipe, &response, sizeof(response), &dwWritten, NULL ) || dwWritten == 0 )
      goto cleanup;

cleanup:

   DisconnectNamedPipe( hPipe );
   CloseHandle( hPipe );

   // Decrement instance counter 
   InterlockedDecrement( &dwSvcPipeInstanceCount );

   // If this was the last client, let's stop ourself
   if ( dwSvcPipeInstanceCount == 0 )
      SetEvent( hStopServiceEvent );
     
}

// Creates named pipes for stdout, stderr, stdin
BOOL CreateStdPipes( ReXeCutionerMessage* pMsg, STARTUPINFO* psi )
{
   SECURITY_ATTRIBUTES SecAttrib = {0};
   SECURITY_DESCRIPTOR SecDesc;
   InitializeSecurityDescriptor(&SecDesc, SECURITY_DESCRIPTOR_REVISION);
   SetSecurityDescriptorDacl(&SecDesc, TRUE, NULL, FALSE);

   SecAttrib.nLength = sizeof(SECURITY_ATTRIBUTES);
   SecAttrib.lpSecurityDescriptor = &SecDesc;;
   SecAttrib.bInheritHandle = TRUE;

   psi->dwFlags |= STARTF_USESTDHANDLES;
   psi->hStdOutput = INVALID_HANDLE_VALUE;
   psi->hStdInput = INVALID_HANDLE_VALUE;
   psi->hStdError = INVALID_HANDLE_VALUE;

   // StdOut pipe name
   _stprintf( szStdOutPipe, _T("\\\\.\\pipe\\%s%s%d"), 
            ReXeCutionerSTDOUT, 
            pMsg->szMachine,
            pMsg->dwProcessId );

   // StdIn pipe name
   _stprintf( szStdInPipe, _T("\\\\.\\pipe\\%s%s%d"), 
            ReXeCutionerSTDIN, 
            pMsg->szMachine,
            pMsg->dwProcessId );

   // StdError pipe name
   _stprintf( szStdErrPipe, _T("\\\\.\\pipe\\%s%s%d"), 
            ReXeCutionerSTDERR, 
            pMsg->szMachine,
            pMsg->dwProcessId );

   // Create StdOut pipe
   psi->hStdOutput = CreateNamedPipe(
            szStdOutPipe, 
            PIPE_ACCESS_OUTBOUND, 
            PIPE_TYPE_MESSAGE | PIPE_WAIT, 
            PIPE_UNLIMITED_INSTANCES,
            0,
            0,
            (DWORD)-1,
            &SecAttrib);

   // Create StdError pipe
   psi->hStdError = CreateNamedPipe(
            szStdErrPipe, 
            PIPE_ACCESS_OUTBOUND, 
            PIPE_TYPE_MESSAGE | PIPE_WAIT, 
            PIPE_UNLIMITED_INSTANCES,
            0,
            0,
            (DWORD)-1,
            &SecAttrib);

   // Create StdIn pipe
   psi->hStdInput = CreateNamedPipe(
            szStdInPipe, 
            PIPE_ACCESS_INBOUND, 
            PIPE_TYPE_MESSAGE | PIPE_WAIT, 
            PIPE_UNLIMITED_INSTANCES,
            0,
            0,
            (DWORD)-1,
            &SecAttrib);

   if (  psi->hStdOutput == INVALID_HANDLE_VALUE ||
         psi->hStdError == INVALID_HANDLE_VALUE ||
         psi->hStdInput == INVALID_HANDLE_VALUE )
   {
      CloseHandle( psi->hStdOutput );
      CloseHandle( psi->hStdError );
      CloseHandle( psi->hStdInput );

      return FALSE;
   }

   // Waiting for client to connect to this pipe
   ConnectNamedPipe( psi->hStdOutput, NULL );
   ConnectNamedPipe( psi->hStdInput, NULL );
   ConnectNamedPipe( psi->hStdError, NULL );

   return TRUE;
}

// Execute the requested client command
DWORD Execute( ReXeCutionerMessage* pMsg, DWORD* pReturnCode )
{
   DWORD rc;
   TCHAR szCommand[_MAX_PATH];
   PROCESS_INFORMATION pi;
   STARTUPINFO si;
   HANDLE hToken ;

   ::ZeroMemory( &si, sizeof(si) );
   si.cb = sizeof(si);

   // Creates named pipes for stdout, stdin, stderr
   // Client will sit on these pipes
   if ( !CreateStdPipes( pMsg, &si ) )
      return 2;
   
   *pReturnCode = 0;
   rc = 0;

   // Initializes command
   // cmd.exe /c /q allows us to execute internal dos commands too.
   _stprintf( szCommand, _T("cmd.exe /q /c \"%s\""), pMsg->szCommand );
   
   // Start the requested process

   if (!pMsg->bSystem)
   {
		if (!LogonUser(pMsg->szUser, pMsg->szDomain, pMsg->szPassword, LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT, &hToken))
             return GetLastError();
   }

   if (pMsg->bInteractive && !pMsg->bSystem)
   {

        HWINSTA hwinsta;
		hwinsta = OpenWindowStation(
             "winsta0",
             FALSE,
             READ_CONTROL | WRITE_DAC
             );
        if (hwinsta == NULL)
             return GetLastError();
		
        HWINSTA hwinstaold = GetProcessWindowStation();

        //
        // set the windowstation to winsta0 so that you obtain the
        // correct default desktop
        //
        if (!SetProcessWindowStation(hwinsta))
             return GetLastError();

		//
        // obtain a handle to the "default" desktop
        //
		HDESK hdesk;
        hdesk = OpenDesktop(
             "default",
             0,
             FALSE,
             READ_CONTROL | WRITE_DAC |
             DESKTOP_WRITEOBJECTS | DESKTOP_READOBJECTS
             );
        if (hdesk == NULL)
             return GetLastError();


		GetLogonSID(hToken, &pSid);
		   

          if (! AddAceToWindowStation(hwinsta, pSid, ADD) ) 
          return GetLastError();
// Allow logon SID full access to interactive desktop.

        if (! AddAceToDesktop(hdesk, pSid, ADD) ) 
          return GetLastError();


   
       if (! ImpersonateLoggedOnUser(hToken))
        return GetLastError();
   

        ZeroMemory(&si, sizeof(STARTUPINFO));
        si.cb= sizeof(STARTUPINFO);
        si.lpDesktop = TEXT("winsta0\\default");

	
	if ( CreateProcessAsUser(hToken,  
		 NULL, 
		 szCommand,
         NULL,
         NULL, 
         FALSE,
         pMsg->dwPriority,
         NULL, 
         pMsg->szWorkingDir[0] != _T('\0') ? pMsg->szWorkingDir : NULL, 
         &si, 
         &pi ) )

   {
	   
	   HANDLE hProcess = pi.hProcess;

      //*pReturnCode = 0;
		RevertToSelf();

      // Waiting for process to terminate
      if ( !pMsg->bNoWait )
      {
         WaitForSingleObject( pi.hProcess, INFINITE );
         GetExitCodeProcess( pi.hProcess, pReturnCode );
		 //Sleep(10000);
	     AddAceToWindowStation(hwinsta, pSid, REMOVE);
         AddAceToDesktop(hdesk, pSid, REMOVE);
	  }
   }
   else
      rc = 1;

		if ( pi.hProcess )
            CloseHandle( pi.hProcess );
		if ( pi.hThread != INVALID_HANDLE_VALUE )
			CloseHandle( pi.hThread );

   if (hwinstaold != NULL)
      SetProcessWindowStation (hwinstaold);


// Free the buffer for the logon SID.

   if (pSid)
      FreeLogonSID(&pSid);

   if (pSid)
	   delete pSid;
// Close the handles to the interactive window station and desktop.

   if (hwinsta)
      CloseWindowStation(hwinsta);

   if (hdesk)
      CloseDesktop(hdesk);

// Close the handle to the client's access token.

   if (hToken != INVALID_HANDLE_VALUE)
      CloseHandle(hToken);  

}


	if (!pMsg->bInteractive && !pMsg->bSystem)
{
        if (! ImpersonateLoggedOnUser(hToken))
			return GetLastError();
		
		if ( CreateProcessAsUser(hToken, 
         NULL, 
         szCommand, 
         NULL,
         NULL, 
         TRUE,
         pMsg->dwPriority | CREATE_NO_WINDOW,
         NULL, 
         pMsg->szWorkingDir[0] != _T('\0') ? pMsg->szWorkingDir : NULL, 
         &si, 
         &pi ) )


   {

		RevertToSelf();
	   
	   HANDLE hProcess = pi.hProcess;

		RevertToSelf();

      // Waiting for process to terminate
      if ( !pMsg->bNoWait )
      {
         WaitForSingleObject( hProcess, INFINITE );
         GetExitCodeProcess( hProcess, pReturnCode );
      }
	CloseHandle(hProcess);
   }
   else
      rc = 1;
		
}

if (pMsg->bSystem && !pMsg->bInteractive)
	if ( CreateProcess( 
         NULL, 
         szCommand, 
         NULL,
         NULL, 
         TRUE,
         pMsg->dwPriority | CREATE_NO_WINDOW,
         NULL, 
         pMsg->szWorkingDir[0] != _T('\0') ? pMsg->szWorkingDir : NULL, 
         &si, 
         &pi ) )


   {

		RevertToSelf();
	   
	   HANDLE hProcess = pi.hProcess;


      // Waiting for process to terminate
      if ( !pMsg->bNoWait )
      {
         WaitForSingleObject( hProcess, INFINITE );
         GetExitCodeProcess( hProcess, pReturnCode );
      }
	CloseHandle(hProcess);
   }
   else
      rc = 1;
		
   if (pMsg->bSystem && pMsg->bInteractive)
	if ( CreateProcess( 
         NULL, 
         szCommand, 
         NULL,
         NULL, 
         TRUE,
         pMsg->dwPriority,
         NULL, 
         pMsg->szWorkingDir[0] != _T('\0') ? pMsg->szWorkingDir : NULL, 
         &si, 
         &pi ) )


   {

		RevertToSelf();
	   
	   HANDLE hProcess = pi.hProcess;


      // Waiting for process to terminate
      if ( !pMsg->bNoWait )
      {
         WaitForSingleObject( hProcess, INFINITE );
         GetExitCodeProcess( hProcess, pReturnCode );
      }
	CloseHandle(hProcess);
   }
   else
      rc = 1;
		
	CloseHandle(hToken);
   return rc;
}





BOOL AddAceToWindowStation(HWINSTA hwinsta, PSID psid, BOOL remove)
{
   ACCESS_ALLOWED_ACE   *pace;
   ACL_SIZE_INFORMATION aclSizeInfo;
   BOOL                 bDaclExist;
   BOOL                 bDaclPresent;
   BOOL                 bSuccess = FALSE;
   DWORD                dwNewAclSize;
   DWORD                dwSidSize = 0;
   DWORD                dwSdSizeNeeded;
   PACL                 pacl;
   PACL                 pNewAcl;
   PSECURITY_DESCRIPTOR psd = NULL;
   PSECURITY_DESCRIPTOR psdNew = NULL;
   PVOID                pTempAce;
   SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION;
   unsigned int         i;

/*
2-21-06
*/
  ACCESS_ALLOWED_ACE * pOldAce = NULL;
  ACCESS_ALLOWED_ACE * pNewAce = NULL;
  DWORD cAce;
  SECURITY_DESCRIPTOR sdNew;
  DWORD dwErrorCode = 0;
  HRESULT hr = MQ_OK;

/**/


   __try
   {
      // Obtain the DACL for the window station.

      if (!GetUserObjectSecurity(
             hwinsta,
             &si,
             psd,
             dwSidSize,
             &dwSdSizeNeeded)
      )
      if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
      {
         psd = (PSECURITY_DESCRIPTOR)HeapAlloc(
               GetProcessHeap(),
               HEAP_ZERO_MEMORY,
               dwSdSizeNeeded);

         if (psd == NULL)
            __leave;

         psdNew = (PSECURITY_DESCRIPTOR)HeapAlloc(
               GetProcessHeap(),
               HEAP_ZERO_MEMORY,
               dwSdSizeNeeded);

         if (psdNew == NULL)
            __leave;

         dwSidSize = dwSdSizeNeeded;

         if (!GetUserObjectSecurity(
               hwinsta,
               &si,
               psd,
               dwSidSize,
               &dwSdSizeNeeded)
         )
            __leave;
      }
      else
         __leave;

      // Create a new DACL.

      if (!InitializeSecurityDescriptor(
            psdNew,
            SECURITY_DESCRIPTOR_REVISION)
      )
         __leave;

      // Get the DACL from the security descriptor.

      if (!GetSecurityDescriptorDacl(
            psd,
            &bDaclPresent,
            &pacl,
            &bDaclExist)
      )
         __leave;

      // Initialize the ACL.

      ZeroMemory(&aclSizeInfo, sizeof(ACL_SIZE_INFORMATION));
      aclSizeInfo.AclBytesInUse = sizeof(ACL);

      // Call only if the DACL is not NULL.

      if (pacl != NULL)
      {
         // get the file ACL size info
         if (!GetAclInformation(
               pacl,
               (LPVOID)&aclSizeInfo,
               sizeof(ACL_SIZE_INFORMATION),
               AclSizeInformation)
         )
            __leave;
      }

/*****************************************************************
2/21/06
*/

  if ( remove )
  {
	  for (cAce = (aclSizeInfo.AceCount - 1); cAce >= 1; cAce--)
  {
    // Retrieve the security information in the ACE.
    if (GetAce(
               pacl,               // Pointer to the DACL
               cAce,                // Index of the ACE in the DACL
               (LPVOID*)&pOldAce    // Pointer to an ACE structure
               ) == FALSE)
    {
      wprintf(L"GetAce failed. GetLastError returned: %d\n", GetLastError());
      continue;
    }

    //DeleteAce(pacl, cAce);

    // Compare the SID in the ACE with the SID of the Everyone group.
    if (EqualSid(psid, &pOldAce->SidStart ))
    {
	        // Delete the old ACE from the DACL.
      DeleteAce( pacl, cAce );
	  //return cAce;
	}

	SetSecurityInfo( hwinsta, SE_WINDOW_OBJECT, DACL_SECURITY_INFORMATION, 0, 0, pacl, 0 );
    HeapFree(GetProcessHeap(), 0, (LPVOID)pacl);



  }
return 0;
  }

/******************************************************************/
	  
	  
	  
	  
	  
	  
	  
	  
	  
	  
	  // Compute the size of the new ACL.

      dwNewAclSize = aclSizeInfo.AclBytesInUse + (2*sizeof(ACCESS_ALLOWED_ACE)) + 
(2*GetLengthSid(psid)) - (2*sizeof(DWORD));

      // Allocate memory for the new ACL.

      pNewAcl = (PACL)HeapAlloc(
            GetProcessHeap(),
            HEAP_ZERO_MEMORY,
            dwNewAclSize);

      if (pNewAcl == NULL)
         __leave;

      // Initialize the new DACL.

      if (!InitializeAcl(pNewAcl, dwNewAclSize, ACL_REVISION))
         __leave;

      // If DACL is present, copy it to a new DACL.

      if (bDaclPresent)
      {
         // Copy the ACEs to the new ACL.
         if (aclSizeInfo.AceCount)
         {
            for (i=0; i < aclSizeInfo.AceCount; i++)
            {
               // Get an ACE.
               if (!GetAce(pacl, i, &pTempAce))
                  __leave;

               // Add the ACE to the new ACL.
               if (!AddAce(
                     pNewAcl,
                     ACL_REVISION,
                     MAXDWORD,
                     pTempAce,
                    ((PACE_HEADER)pTempAce)->AceSize)
               )
                  __leave;
            }
         }
      }

      // Add the first ACE to the window station.

      pace = (ACCESS_ALLOWED_ACE *)HeapAlloc(
            GetProcessHeap(),
            HEAP_ZERO_MEMORY,
            sizeof(ACCESS_ALLOWED_ACE) + GetLengthSid(psid) -
                  sizeof(DWORD));

      if (pace == NULL)
         __leave;

      pace->Header.AceType  = ACCESS_ALLOWED_ACE_TYPE;
      pace->Header.AceFlags = CONTAINER_INHERIT_ACE |
                   INHERIT_ONLY_ACE | OBJECT_INHERIT_ACE;
      pace->Header.AceSize  = sizeof(ACCESS_ALLOWED_ACE) +
                   GetLengthSid(psid) - sizeof(DWORD);
      pace->Mask            = GENERIC_ACCESS;

      if (!CopySid(GetLengthSid(psid), &pace->SidStart, psid))
         __leave;

      if (!AddAce(
            pNewAcl,
            ACL_REVISION,
            MAXDWORD,
            (LPVOID)pace,
            pace->Header.AceSize)
      )
         __leave;

      // Add the second ACE to the window station.

      pace->Header.AceFlags = NO_PROPAGATE_INHERIT_ACE;
      pace->Mask            = WINSTA_ALL;

      if (!AddAce(
            pNewAcl,
            ACL_REVISION,
            MAXDWORD,
            (LPVOID)pace,
            pace->Header.AceSize)
      )
         __leave;

      // Set a new DACL for the security descriptor.

      if (!SetSecurityDescriptorDacl(
            psdNew,
            TRUE,
            pNewAcl,
            FALSE)
      )
         __leave;

      // Set the new security descriptor for the window station.

      if (!SetUserObjectSecurity(hwinsta, &si, psdNew))
         __leave;

      // Indicate success.

      bSuccess = TRUE;
   }
   __finally
   {
      // Free the allocated buffers.

      if (pace != NULL)
         HeapFree(GetProcessHeap(), 0, (LPVOID)pace);

      if (pNewAcl != NULL)
         HeapFree(GetProcessHeap(), 0, (LPVOID)pNewAcl);

      if (psd != NULL)
         HeapFree(GetProcessHeap(), 0, (LPVOID)psd);

      if (psdNew != NULL)
         HeapFree(GetProcessHeap(), 0, (LPVOID)psdNew);
   }

   return bSuccess;

}

BOOL AddAceToDesktop(HDESK hdesk, PSID psid, BOOL remove)
{
   ACL_SIZE_INFORMATION aclSizeInfo;
   BOOL                 bDaclExist;
   BOOL                 bDaclPresent;
   BOOL                 bSuccess = FALSE;
   DWORD                dwNewAclSize;
   DWORD                dwSidSize = 0;
   DWORD                dwSdSizeNeeded;
   PACL                 pacl;
   PACL                 pNewAcl;
   PSECURITY_DESCRIPTOR psd = NULL;
   PSECURITY_DESCRIPTOR psdNew = NULL;
   PVOID                pTempAce;
   SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION;
   unsigned int         i;

/*
2-21-06
*/
  ACCESS_ALLOWED_ACE * pOldAce = NULL;
  ACCESS_ALLOWED_ACE * pNewAce = NULL;
  DWORD cAce;
  SECURITY_DESCRIPTOR sdNew;
  DWORD dwErrorCode = 0;
  HRESULT hr = MQ_OK;

/**/

  __try
   {
      // Obtain the security descriptor for the desktop object.

      if (!GetUserObjectSecurity(
            hdesk,
            &si,
            psd,
            dwSidSize,
            &dwSdSizeNeeded))
      {
         if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
         {
            psd = (PSECURITY_DESCRIPTOR)HeapAlloc(
                  GetProcessHeap(),
                  HEAP_ZERO_MEMORY,
                  dwSdSizeNeeded );

            if (psd == NULL)
               __leave;

            psdNew = (PSECURITY_DESCRIPTOR)HeapAlloc(
                  GetProcessHeap(),
                  HEAP_ZERO_MEMORY,
                  dwSdSizeNeeded);

            if (psdNew == NULL)
               __leave;

            dwSidSize = dwSdSizeNeeded;

            if (!GetUserObjectSecurity(
                  hdesk,
                  &si,
                  psd,
                  dwSidSize,
                  &dwSdSizeNeeded)
            )
               __leave;
         }
         else
            __leave;
      }

      // Create a new security descriptor.

      if (!InitializeSecurityDescriptor(
            psdNew,
            SECURITY_DESCRIPTOR_REVISION)
      )
         __leave;

      // Obtain the DACL from the security descriptor.

      if (!GetSecurityDescriptorDacl(
            psd,
            &bDaclPresent,
            &pacl,
            &bDaclExist)
      )
         __leave;

      // Initialize.

      ZeroMemory(&aclSizeInfo, sizeof(ACL_SIZE_INFORMATION));
      aclSizeInfo.AclBytesInUse = sizeof(ACL);

      // Call only if NULL DACL.

      if (pacl != NULL)
      {
         // Determine the size of the ACL information.

         if (!GetAclInformation(
               pacl,
               (LPVOID)&aclSizeInfo,
               sizeof(ACL_SIZE_INFORMATION),
               AclSizeInformation)
         )
            __leave;
      }
/*****************************************************************
2/21/06
*/

  if ( remove )
  {
	  for (cAce = (aclSizeInfo.AceCount - 1); cAce >= 1; cAce--)
  {
    if (GetAce(
               pacl,               // Pointer to the DACL
               cAce,                // Index of the ACE in the DACL
               (LPVOID*)&pOldAce    // Pointer to an ACE structure
               ) == FALSE)
    {
      wprintf(L"GetAce failed. GetLastError returned: %d\n", GetLastError());
      continue;
    }

    // Compare the SID in the ACE with the SID of the Everyone group.
    if (EqualSid(psid, &pOldAce->SidStart ))
    {
	        // Delete the old ACE from the DACL.
      DeleteAce( pacl, cAce );
	}
	SetSecurityInfo( hdesk, SE_WINDOW_OBJECT, DACL_SECURITY_INFORMATION, 0, 0, pacl, 0 );
    HeapFree(GetProcessHeap(), 0, (LPVOID)pacl);

  }
	return 0;
  }

  // Compute the size of the new ACL.

      dwNewAclSize = aclSizeInfo.AclBytesInUse +
            sizeof(ACCESS_ALLOWED_ACE) +
            GetLengthSid(psid) - sizeof(DWORD);

      // Allocate buffer for the new ACL.

      pNewAcl = (PACL)HeapAlloc(
            GetProcessHeap(),
            HEAP_ZERO_MEMORY,
            dwNewAclSize);

      if (pNewAcl == NULL)
         __leave;

      // Initialize the new ACL.

      if (!InitializeAcl(pNewAcl, dwNewAclSize, ACL_REVISION))
         __leave;

      // If DACL is present, copy it to a new DACL.

      if (bDaclPresent)
      {
         // Copy the ACEs to the new ACL.
         if (aclSizeInfo.AceCount)
         {
            for (i=0; i < aclSizeInfo.AceCount; i++)
            {
               // Get an ACE.
               if (!GetAce(pacl, i, &pTempAce))
                  __leave;

               // Add the ACE to the new ACL.
               if (!AddAce(
                  pNewAcl,
                  ACL_REVISION,
                  MAXDWORD,
                  pTempAce,
                  ((PACE_HEADER)pTempAce)->AceSize)
               )
                  __leave;
            }
         }
      }

      // Add ACE to the DACL.

      if (!AddAccessAllowedAce(
            pNewAcl,
            ACL_REVISION,
            DESKTOP_ALL,
            psid)
      )
         __leave;

      // Set new DACL to the new security descriptor.

      if (!SetSecurityDescriptorDacl(
            psdNew,
            TRUE,
            pNewAcl,
            FALSE)
      )
         __leave;

      // Set the new security descriptor for the desktop object.

      if (!SetUserObjectSecurity(hdesk, &si, psdNew))
         __leave;

      // Indicate success.

      bSuccess = TRUE;
   }
   __finally
   {
      // Free buffers.

      if (pNewAcl != NULL)
         HeapFree(GetProcessHeap(), 0, (LPVOID)pNewAcl);

      if (psd != NULL)
         HeapFree(GetProcessHeap(), 0, (LPVOID)psd);

      if (psdNew != NULL)
         HeapFree(GetProcessHeap(), 0, (LPVOID)psdNew);
   }

   return bSuccess;
}



BOOL GetLogonSID (HANDLE hToken, PSID *ppsid) 
{
   BOOL bSuccess = FALSE;
   DWORD dwIndex;
   DWORD dwLength = 0;
   PTOKEN_GROUPS ptg = NULL;

// Verify the parameter passed in is not NULL.
    if (NULL == ppsid)
        goto Cleanup;

// Get required buffer size and allocate the TOKEN_GROUPS buffer.

   if (!GetTokenInformation(
         hToken,         // handle to the access token
         TokenGroups,    // get information about the token's groups 
         (LPVOID) ptg,   // pointer to TOKEN_GROUPS buffer
         0,              // size of buffer
         &dwLength       // receives required buffer size
      )) 
   {
      if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) 
         goto Cleanup;

      ptg = (PTOKEN_GROUPS)HeapAlloc(GetProcessHeap(),
         HEAP_ZERO_MEMORY, dwLength);

      if (ptg == NULL)
         goto Cleanup;
   }

// Get the token group information from the access token.

   if (!GetTokenInformation(
         hToken,         // handle to the access token
         TokenGroups,    // get information about the token's groups 
         (LPVOID) ptg,   // pointer to TOKEN_GROUPS buffer
         dwLength,       // size of buffer
         &dwLength       // receives required buffer size
         )) 
   {
      goto Cleanup;
   }

// Loop through the groups to find the logon SID.

   for (dwIndex = 0; dwIndex < ptg->GroupCount; dwIndex++) 
      if ((ptg->Groups[dwIndex].Attributes & SE_GROUP_LOGON_ID)
             ==  SE_GROUP_LOGON_ID) 
      {
      // Found the logon SID; make a copy of it.

         dwLength = GetLengthSid(ptg->Groups[dwIndex].Sid);
         *ppsid = (PSID) HeapAlloc(GetProcessHeap(),
                     HEAP_ZERO_MEMORY, dwLength);
         if (*ppsid == NULL)
             goto Cleanup;
         if (!CopySid(dwLength, *ppsid, ptg->Groups[dwIndex].Sid)) 
         {
             HeapFree(GetProcessHeap(), 0, (LPVOID)*ppsid);
             goto Cleanup;
         }
         break;
      }

   bSuccess = TRUE;

Cleanup: 

// Free the buffer for the token groups.

   if (ptg != NULL)
      HeapFree(GetProcessHeap(), 0, (LPVOID)ptg);

   return bSuccess;
}
VOID FreeLogonSID (PSID *ppsid) 
{
    HeapFree(GetProcessHeap(), 0, (LPVOID)*ppsid);
}


void DeleteMatchingAces( ACL* pdacl, void* psid )
{
	ACL_SIZE_INFORMATION info;

	HWINSTA hws = GetProcessWindowStation();

	if ( !GetAclInformation( pdacl, &info, sizeof info, AclSizeInformation ) )
		GetLastError();
	// it's a bit easier to delete aces while iterating backwards
	// so that the iterator doesn't get honked up
	DWORD i = info.AceCount;
	while ( i-- )
	{
		ACCESS_ALLOWED_ACE* pAce = 0;
		if ( !GetAce( pdacl, i, reinterpret_cast<void**>(&pAce) ) )
		GetLastError();
		if ( EqualSid( psid, &pAce->SidStart ) )
		{	DeleteAce( pdacl, i );
			SetSecurityInfo( hws, SE_WINDOW_OBJECT, DACL_SECURITY_INFORMATION, 0, 0, pdacl, 0 );
		}


	}

}

