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
// ReXeCutioner.cpp
//
// History:
//
//    3/27/2001      Initial version
//    2/08/2006      Chris Pelphrey - Major Revision - 2.0
//
//		Email: riscchip@gmail.com
//
//                       Added /interactive and /system switches
//	                     Interactive on the remote window station and credential usage mods Microsoft SDK Code
//                       Microsoft Licensing "The sample code in this documentation is provided "as is" without warranty of any kind.
//                                           "You are solely responsible for your use of the sample code and for any results from your use of the sample code."
//		Changed name to ReXeCutioner
//		XOR'd sensitive portions of the comm's data structure to prevent login credentials from 
//		being passed across the network in plain text
//		
//
///////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <winsvc.h>
#include <tchar.h>
#include <lmcons.h>
#include <stdio.h>
#include <io.h>
#include <direct.h>
#include <string.h>

#include <stdlib.h>
#include <process.h>
#include <iostream>
#include "resource.h"
#include "ReXeCutioner.h"


#define BUFFERSIZE   0x100

TCHAR szThisMachine[BUFFERSIZE] = _T("");
TCHAR szPassword[BUFFERSIZE] = _T("");
TCHAR szArguments[BUFFERSIZE] = _T("");
TCHAR szConsoleTitle[BUFFERSIZE] = _T("");;
TCHAR szLogPath[MAX_PATH] = _T("");
TCHAR szEmpty[MAX_PATH] = _T("");

LPCTSTR lpszMachine = NULL;
LPCTSTR lpszPassword = NULL;
LPCTSTR lpszUser = NULL;
LPCTSTR lpszCommandExe = NULL;

HANDLE hCommandPipe = INVALID_HANDLE_VALUE;
HANDLE hRemoteStdOutputPipe = INVALID_HANDLE_VALUE;
HANDLE hRemoteStdInputPipe = INVALID_HANDLE_VALUE;
HANDLE hRemoteStdErrorPipe = INVALID_HANDLE_VALUE;
void logToFile(LPCTSTR buffer, DWORD numBytes);

// Show program information
void ShowProgramInfo()
{
   StdOutput( _T("xCmd v1.0 for NT4/2000 - executes commands remotely\n") );
   StdOutput( _T("Freeware! 2001 Zoltan Csizmadia, zoltan_csizmadia@yahoo.com\n") );
   StdOutput( _T("***********************************************************\n") )
   StdOutput( _T("ReXeCutioner v2.0 for XP - executes commands remotely\n") );
   StdOutput( _T("Currently only tested on XP SP2\n") );
   StdOutput( _T("2006 Chris Pelphrey, riscchip@gmail.com\n") );
   StdOutput( _T("***********************************************************\n") )
}

// Help
void ShowHelp()
{
   StdOutput( _T("Usage: ReXeCutioner.exe \\\\computer [options] command/exe arguments\n") );
   StdOutput( _T("\n") );
   StdOutput( _T("Options:\n") );
   StdOutput( _T("   /D:directory           Set working directory\n") );
   StdOutput( _T("                          Default: Remote \"%SystemRoot%\\System32\"\n") );
   StdOutput( _T("   /IDLE                  Idle priority class\n") );
   StdOutput( _T("   /NORMAL                Normal priority class\n") );
   StdOutput( _T("   /HIGH                  High priority class\n") );
   StdOutput( _T("   /REALTIME              Realtime priority class\n") );
   StdOutput( _T("   /C                     Copy the specified program to the remote machine's\n") );
   StdOutput( _T("                          \"%SystemRoot%\\System32\" directory\n") );
   StdOutput( _T("                          Commands's exe file must be absolute to local machine\n") );
   StdOutput( _T("   /USER:user             User for remote connection\n") );
   StdOutput( _T("   /PWD:{password|*}      Password for remote connection\n") );
   StdOutput( _T("   /NOWAIT                Don't wait for remote process to terminate\n") );
   StdOutput( _T("   /INTERACTIVE           Run the process so that it interacts with the desktop of the remote system\n") );
   StdOutput( _T("   /SYSTEM                Run the process in the SYSTEM ACCOUNT on the remote system\n") );
   StdOutput( _T("   You must use either /SYSTEM OR /user: AND /pwd: to specify whose credentials to run the application under.\n") );
   StdOutput( _T("\n") );
   StdOutput( _T("Examples:\n") );
   StdOutput( _T("   ReXeCutioner.exe \\\\remote cmd       // Starts a \"telnet\" client\n") );
   StdOutput( _T("   ReXeCutioner.exe \\\\remote /nowait runme.exe\n") );
   StdOutput( _T("   ReXeCutioner.exe \\\\remote /user:administrator dir c:\\\n") );
   StdOutput( _T("   ReXeCutioner.exe \\\\remote /user:somebody /pwd:* /d:d:\\ test1.exe\n") );
   StdOutput( _T("   ReXeCutioner.exe \\\\remote /c /user:somebody /pwd:* /d:d:\\ test2.exe\n") );
   StdOutput( _T("\n") );
   StdOutput( _T("Notes:\n") );
   StdOutput( _T("- Input is passed to remote machine when you press the ENTER.\n") ); 
   StdOutput( _T("- Ctrl-C terminates the remote process\n") );
   StdOutput( _T("- Command and file path arguments have to be absolute to remote machine\n") );
   StdOutput( _T("  If you are using /c option, command exe file path must be absolute to\n") );
   StdOutput( _T("  local machine, but the arguments must be absolute to remote machine\n") );
}

// Check the command line arguments
BOOL IsCmdLineParameter( LPCTSTR lpszParam )
{
   for( int i = 1; i < __argc; i++ )
      if ( __targv[i][0] == _T('\\') )
         continue;
      else
      if ( __targv[i][0] == _T('/') || __targv[i][0] == _T('-') )
      {
         if ( _tcsicmp( __targv[i] + 1, lpszParam ) == 0 )
            return TRUE;
      }
      else
         return FALSE;

   return FALSE;
}

// Check the command line arguments
LPCTSTR GetCmdLineParameterValue( LPCTSTR lpszParam )
{
   DWORD dwParamLength = _tcslen( lpszParam );

   for( int i = 1; i < __argc; i++ )
      if ( __targv[i][0] == _T('\\') )
         continue;
      else
      if ( __targv[i][0] == _T('/') || __targv[i][0] == _T('-') )
      {
         if ( _tcsnicmp( __targv[i] + 1, lpszParam, dwParamLength ) == 0 )
            return __targv[i] + dwParamLength + 1;
      }
      else
         return NULL;

   return NULL;
}

LPCTSTR GetNthNonSwitchParameter( DWORD n, DWORD& argvIndex )
{
   DWORD index = 0;

   for( int i = 1; i < __argc; i++ )
   {
      if ( __targv[i][0] != _T('/') && __targv[i][0] != _T('-') )
         index++;

      if ( index == n )
      {
         argvIndex = i;
         return __targv[i];
      }
   }
   
   return NULL;
}

// Gets the arguments parameter
void GetRemoteCommandArguments( LPTSTR lpszCommandArguments )
{
   DWORD dwIndex = 0;
   lpszCommandArguments[0] = _T('\0');

   if ( GetNthNonSwitchParameter( 3, dwIndex ) != NULL )
      for( int i = dwIndex; i < __argc; i++ )
      {
         _tcscat( lpszCommandArguments, __targv[i] );
         if ( i + 1 < __argc )
            _tcscat( lpszCommandArguments, _T(" ") );
      }
}

// Gets the remote machine parameter
LPCTSTR GetRemoteMachineName()
{
   DWORD dwIndex = 0;
   LPCTSTR lpszMachine = GetNthNonSwitchParameter( 1, dwIndex );

   if ( lpszMachine == NULL )
      return NULL;

   if ( _tcsnicmp( lpszMachine, _T("\\\\"), 2 ) == 0 )
      return lpszMachine;

   return NULL;
}

// Turns off the echo on a console input handle
// Fore example, for password, we don't need echo :)
BOOL EnableEcho( HANDLE handle, BOOL bEcho )
{
   DWORD mode;


   if ( !GetConsoleMode( handle, &mode ) )
      return FALSE;
   
   if ( bEcho )
      mode |= ENABLE_ECHO_INPUT;
   else
      mode &= ~ENABLE_ECHO_INPUT;
   
   return SetConsoleMode( handle, mode );
}

// Gets the password
BOOL PromptForPassword( LPTSTR lpszPwd )
{
   HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
   DWORD dwRead = 0;

   StdOutput( _T("Password: ") );

   // Turn off echo
   if ( EnableEcho( hInput, FALSE ) )
   {
      // Read password from console
      ::ReadConsole( hInput, lpszPwd, BUFFERSIZE, &dwRead, NULL );
      
      // Ignore ENTER (0x0D0A) 
      lpszPwd[max( dwRead-2, 0 )] = _T('\0');
      
      // Turn echo on
      EnableEcho( hInput, TRUE );

      StdOutput( _T("\n\n") );
   }
   else
   {
      //Console input doesn't support echo on/off
      StdOutput( _T("\n") );
      StdError( _T("Couldn't turn off echo to hide password chars.\n") );
   }

   return TRUE;
}

// Set the user and password parameters
// If there is no command line parameter for user/pwd
// we use NULLs
BOOL SetUserAndPassword( BOOL bPromptForPassword )
{
   // Check the command line
   lpszPassword = GetCmdLineParameterValue( _T("pwd:") );
   lpszUser = GetCmdLineParameterValue( _T("user:") );

   if ( lpszUser != NULL && lpszPassword != NULL && !bPromptForPassword )
      if ( _tcscmp( lpszPassword, _T("*") ) == 0 )
         // We found user name, and * as password, which means prompt for password
         bPromptForPassword = TRUE;
      
   if ( bPromptForPassword )
   {
      // We found user name, and * as password, which means prompt for password
      lpszPassword = szPassword;
      if ( !PromptForPassword( szPassword ) )
         return FALSE;
   }
  
   return TRUE;
}

// Establish connection or disconnect remote machine
BOOL EstablishConnection( LPCTSTR lpszRemote, LPCTSTR lpszResource, BOOL bEstablish )
{
   TCHAR szRemoteResource[_MAX_PATH];

   DWORD rc;

   // Remote resource, \\remote\ipc$, remote\admin$, ...
   _stprintf( szRemoteResource, _T("%s\\%s"), lpszRemote, lpszResource );

   //
   // disconnect or connect to the resource, based on bEstablish
   //
   if ( bEstablish ) 
   {
      NETRESOURCE nr;
      nr.dwType = RESOURCETYPE_ANY;
      nr.lpLocalName = NULL;
      nr.lpRemoteName = (LPTSTR)&szRemoteResource;
      nr.lpProvider = NULL;
      
      //Establish connection (using username/pwd)
      rc = WNetAddConnection2( &nr, lpszPassword, lpszUser, FALSE );
   
      switch( rc )
      {
      case ERROR_ACCESS_DENIED:
      case ERROR_INVALID_PASSWORD:
      case ERROR_LOGON_FAILURE:
      case ERROR_SESSION_CREDENTIAL_CONFLICT:
         // Prompt for password if the default(NULL) was not good
         if ( lpszUser != NULL && lpszPassword == NULL )
         {
            StdOutput( _T("Invalid password\n\n") );
            SetUserAndPassword( TRUE );
            StdOutput( _T("Connecting to remote service ... ") );
            //Establish connection (using username/pwd) again
            rc = WNetAddConnection2( &nr, lpszPassword, lpszUser, FALSE );
         }
         break;
      }
   }
   else 
      // Disconnect
      rc = WNetCancelConnection2( szRemoteResource, 0, NULL );
   
   if ( rc == NO_ERROR ) 
      return TRUE; // indicate success

   SetLastError( rc );

   return FALSE;
}

// Copies the command's exe file to remote machine (\\remote\ADMIN$\System32)
// This function called, if the /c option is used
BOOL CopyExeToRemote()
{
   if ( !IsCmdLineParameter(_T("c")) )
      return TRUE;

   TCHAR drive[_MAX_DRIVE];
   TCHAR dir[_MAX_DIR];
   TCHAR fname[_MAX_FNAME];
   TCHAR ext[_MAX_EXT];
   TCHAR szRemoteResource[_MAX_PATH];

   // Gets the file name and extension
   _tsplitpath( lpszCommandExe, drive, dir, fname, ext );
   
   _stprintf( szRemoteResource, _T("%s\\ADMIN$\\System32\\%s%s"), lpszMachine, fname, ext );

   // Copy the Command's exe file to \\remote\ADMIN$\System32
   return CopyFile( lpszCommandExe, szRemoteResource, FALSE );
}

// Copy the service executable to remote machine
BOOL CopySvcExeToRemoteMachine()
{
   DWORD dwWritten = 0;

   HMODULE hInstance = ::GetModuleHandle(NULL);

   // Find the binary file in resources
   HRSRC hSvcExecutableRes = ::FindResource( 
               hInstance, 
               MAKEINTRESOURCE(IDR_ReXeCutionerSVC), 
               _T("ReXeCutionerSVC") );

   HGLOBAL hSvcExecutable = ::LoadResource( 
               hInstance, 
               hSvcExecutableRes );

   LPVOID pSvcExecutable = ::LockResource( hSvcExecutable );

   if ( pSvcExecutable == NULL )
      return FALSE;

   DWORD dwSvcExecutableSize = ::SizeofResource(
               hInstance,
               hSvcExecutableRes );

   TCHAR szSvcExePath[_MAX_PATH];

   _stprintf( szSvcExePath, _T("%s\\ADMIN$\\System32\\%s"), lpszMachine, ReXeCutionerSVCEXE );

   // Copy binary file from resources to \\remote\ADMIN$\System32
   HANDLE hFileSvcExecutable = CreateFile( 
            szSvcExePath,
            GENERIC_WRITE,
            0,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL );

   if ( hFileSvcExecutable == INVALID_HANDLE_VALUE )
      return FALSE;
   
   WriteFile( hFileSvcExecutable, pSvcExecutable, dwSvcExecutableSize, &dwWritten, NULL );

   CloseHandle( hFileSvcExecutable );
   
   return dwWritten == dwSvcExecutableSize;
}

// Installs and starts the remote service on remote machine
BOOL InstallAndStartRemoteService()
{
   // Open remote Service Manager
   SC_HANDLE hSCM = ::OpenSCManager( lpszMachine, NULL, SC_MANAGER_ALL_ACCESS);

   if (hSCM == NULL)
      return FALSE;
   
   // Maybe it's already there and installed, let's try to run
   SC_HANDLE hService =::OpenService( hSCM, SERVICENAME, SERVICE_ALL_ACCESS );

   // Creates service on remote machine, if it's not installed yet
   if ( hService == NULL )
      if ( IsCmdLineParameter(_T("INTERACTIVE")) && IsCmdLineParameter(_T("SYSTEM"))  )
          hService = ::CreateService(
	        hSCM, SERVICENAME, LONGSERVICENAME,
			SERVICE_ALL_ACCESS, 
	        SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS,
			SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
	         _T("%SystemRoot%\\system32\\")ReXeCutionerSVCEXE,
	        NULL, NULL, NULL, NULL, NULL );
	  else 
          hService = ::CreateService(
	        hSCM, SERVICENAME, LONGSERVICENAME,
			SERVICE_ALL_ACCESS, 
	        SERVICE_WIN32_OWN_PROCESS, /* | SERVICE_INTERACTIVE_PROCESS, */
			SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
	         _T("%SystemRoot%\\system32\\")ReXeCutionerSVCEXE,
	        NULL, NULL, NULL, NULL, NULL );

    
   if (hService == NULL)
   {
      ::CloseServiceHandle(hSCM);
      return FALSE;
   }


   // Start service
   if ( !StartService( hService, 0, NULL ) )
      return FALSE;

   ::CloseServiceHandle(hService);
   ::CloseServiceHandle(hSCM);

   return TRUE;
}

// Connects to tehe remote service
BOOL ConnectToRemoteService( DWORD dwRetry, DWORD dwRetryTimeOut )
{
   TCHAR szPipeName[_MAX_PATH] = _T("");

   // Remote service communication pipe name
   _stprintf( szPipeName, _T("%s\\pipe\\%s"), lpszMachine, ReXeCutionerCOMM );

   SECURITY_ATTRIBUTES SecAttrib = {0};
   SECURITY_DESCRIPTOR SecDesc;
   InitializeSecurityDescriptor(&SecDesc, SECURITY_DESCRIPTOR_REVISION);
   SetSecurityDescriptorDacl(&SecDesc, TRUE, NULL, TRUE);

   SecAttrib.nLength = sizeof(SECURITY_ATTRIBUTES);
   SecAttrib.lpSecurityDescriptor = &SecDesc;;
   SecAttrib.bInheritHandle = TRUE;

   // Connects to the remote service's communication pipe
   while( dwRetry-- )
   {
      if ( WaitNamedPipe( szPipeName, 5000 ) )
      {
         hCommandPipe = CreateFile( 
            szPipeName,
            GENERIC_WRITE | GENERIC_READ, 
            0,
            &SecAttrib, 
            OPEN_EXISTING, 
            FILE_ATTRIBUTE_NORMAL, 
            NULL );

         break;
      }
      else
         // Let's try it again
         Sleep( dwRetryTimeOut );
   }

   return hCommandPipe != INVALID_HANDLE_VALUE;
}

// Fill the communication message structure
// This structure will be transferred to remote machine
//////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOL FillMessage( ReXeCutionerMessage* pMsg )
{
   LPCTSTR lpszWorkingDir = GetCmdLineParameterValue( _T("d:") );

   // Information
   pMsg->dwProcessId = GetCurrentProcessId();
   _tcscpy( pMsg->szMachine, szThisMachine );

   // Command
   if ( !IsCmdLineParameter(_T("c")) )
      _stprintf( pMsg->szCommand, _T("%s %s"), lpszCommandExe, szArguments );
   else
   {
      // We did copy the file to \\remote\admin$\system32
      TCHAR drive[_MAX_DRIVE];
      TCHAR dir[_MAX_DIR];
      TCHAR fname[_MAX_FNAME];
      TCHAR ext[_MAX_EXT];

      _tsplitpath( lpszCommandExe, drive, dir, fname, ext );

      _stprintf( pMsg->szCommand, _T("%s%s %s"), fname, ext, szArguments );
   }

   // Priority
   if ( IsCmdLineParameter( _T("realtime") ) )
      pMsg->dwPriority = REALTIME_PRIORITY_CLASS;
   else
   if ( IsCmdLineParameter( _T("high") ) )
      pMsg->dwPriority = HIGH_PRIORITY_CLASS; 
   else
   if ( IsCmdLineParameter( _T("idle") ) )
      pMsg->dwPriority = IDLE_PRIORITY_CLASS; 
   else
      pMsg->dwPriority = NORMAL_PRIORITY_CLASS; // default

   // No wait
   pMsg->bNoWait = IsCmdLineParameter( _T("nowait") );

   // Working directory
   if ( lpszWorkingDir != NULL )
      _tcscpy( pMsg->szWorkingDir, lpszWorkingDir );

   // Set the console's title
   _stprintf( szConsoleTitle, _T("%s : %s"), lpszMachine, pMsg->szCommand );
   SetConsoleTitle( szConsoleTitle );

      pMsg->bInteractive = IsCmdLineParameter( _T("INTERACTIVE") );
      pMsg->bSystem = IsCmdLineParameter( _T("SYSTEM") );
	  if ( !IsCmdLineParameter(_T("USER:")) )
		_stprintf( pMsg->szDomain, _T("%s"), GetCmdLineParameterValue( _T("USER:") ) );

		char *dom = strtok(pMsg->szDomain, "\\");

		_stprintf( pMsg->szUser, _T("%s"), strtok(NULL, "\0") );
		 _stprintf( pMsg->szDomain, _T("%s"), dom);

	  if ( !IsCmdLineParameter(_T("PWD:")) )
		_stprintf( pMsg->szPassword, _T("%s"), GetCmdLineParameterValue( _T("PWD:") ) );

  	  if ( !IsCmdLineParameter(_T("LOG:")) )
		_stprintf( szLogPath, _T("%s"), GetCmdLineParameterValue( _T("LOG:") ) );


   return TRUE;
}

void logToFile(LPCTSTR buffer, DWORD numBytes)
{

   FILE *stream;
   int  i;
   char buf[_MAX_PATH];
   char comp1[_MAX_PATH];
   char comp2[_MAX_PATH];

   /* Open file in text mode: */

	sprintf(comp1, "%s", szLogPath);
	sprintf(comp2, "%s", _T('\0'));

   if ( strcmp(comp1, comp2) == 0 )
      return;


   sprintf(buf, "%s", szLogPath);


   if( (stream = fopen( buf, "a" )) != NULL )
   {

	  fseek(stream, 0L, SEEK_END);
	  i = sizeof( buffer );
      fwrite(buffer, numBytes, 1, stream );
	  fflush(stream);
      fclose( stream );
   }

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Listens the remote stdout pipe
// Remote process will send its stdout to this pipe
void ListenRemoteStdOutputPipeThread(void*)
{
   HANDLE hOutput = GetStdHandle( STD_OUTPUT_HANDLE );
   TCHAR szBuffer[BUFFERSIZE];
   DWORD dwRead;



   for(;;)	
   { 
      if ( !ReadFile( hRemoteStdOutputPipe, szBuffer, BUFFERSIZE, &dwRead, NULL ) || 
            dwRead == 0 ) 
      {
         DWORD dwErr = GetLastError();
         if ( dwErr == ERROR_NO_DATA)
            break;
      }

      // Handle CLS command, just for fun :)
      switch( szBuffer[0] )
      {
      case 12: //cls
         {
            DWORD dwWritten;
            COORD origin = {0,0};
            CONSOLE_SCREEN_BUFFER_INFO sbi;

            if ( GetConsoleScreenBufferInfo( hOutput, &sbi ) )
            {
               FillConsoleOutputCharacter( 
                     hOutput,
                     _T(' '),
                     sbi.dwSize.X * sbi.dwSize.Y,
                     origin,
                     &dwWritten );

               SetConsoleCursorPosition(
                     hOutput,
                     origin );
            }
         }
         continue;
         break;
      }

      szBuffer[ dwRead / sizeof(TCHAR) ] = _T('\0');

      
      // Send it to our stdout
      StdOutput( szBuffer );
	  logToFile( szBuffer, dwRead );
	  


   } 

   CloseHandle( hRemoteStdOutputPipe );
   hRemoteStdOutputPipe = INVALID_HANDLE_VALUE;

   ::ExitThread(0);
}

// Listens the remote stderr pipe
// Remote process will send its stderr to this pipe
void ListenRemoteStdErrorPipeThread(void*)
{
   TCHAR szBuffer[BUFFERSIZE];
   DWORD dwRead;

   for(;;)	
   { 
      if ( !ReadFile( hRemoteStdErrorPipe, szBuffer, BUFFERSIZE, &dwRead, NULL ) || 
            dwRead == 0 ) 
      {
         DWORD dwErr = GetLastError();
         if ( dwErr == ERROR_NO_DATA)
            break;
      }

      szBuffer[ dwRead / sizeof(TCHAR) ] = _T('\0');
      
      

	  // Write it to our stderr
      StdError( szBuffer );
 	  logToFile( szBuffer, dwRead );

   } 

   CloseHandle( hRemoteStdErrorPipe );

   hRemoteStdErrorPipe = INVALID_HANDLE_VALUE;

   ::ExitThread(0);
}

// Listens our console, and if the user types in something,
// we will pass it to the remote machine.
// ReadConsole return after pressing the ENTER
void ListenRemoteStdInputPipeThread(void*)
{
   HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
   TCHAR szInputBuffer[BUFFERSIZE] = _T("");
   DWORD nBytesRead;
   DWORD nBytesWrote;

   for(;;)
   {
      // Read our console input
      if ( !ReadConsole( hInput, szInputBuffer, BUFFERSIZE, &nBytesRead, NULL ) )
      {
         DWORD dwErr = GetLastError();
         if ( dwErr == ERROR_NO_DATA)
            break;
      }
      

	  
	  // Send it to remote process' stdin
      if ( !WriteFile( hRemoteStdInputPipe, szInputBuffer, nBytesRead, &nBytesWrote, NULL ) )
         break;
  	  logToFile( szInputBuffer, nBytesRead );

   } 

   CloseHandle( hRemoteStdInputPipe );

   hRemoteStdInputPipe = INVALID_HANDLE_VALUE;

   ::ExitThread(0);
}

// Start listening stdout, stderr and stdin
void StartListeningRemoteStdPipes()
{
   // StdOut
   _beginthread( ListenRemoteStdOutputPipeThread, 0, NULL );

   // StdErr
   _beginthread( ListenRemoteStdErrorPipeThread, 0, NULL );

   // StdIn
   _beginthread( ListenRemoteStdInputPipeThread, 0, NULL );
}

// Connects to the remote processe' stdout, stderr and stdin named pipes
BOOL ConnectToRemotePipes( DWORD dwRetryCount, DWORD dwRetryTimeOut )
{
   TCHAR szStdOut[_MAX_PATH];
   TCHAR szStdIn[_MAX_PATH];
   TCHAR szStdErr[_MAX_PATH];

   SECURITY_ATTRIBUTES SecAttrib = {0};
   SECURITY_DESCRIPTOR SecDesc;
   InitializeSecurityDescriptor(&SecDesc, SECURITY_DESCRIPTOR_REVISION);
   SetSecurityDescriptorDacl(&SecDesc, TRUE, NULL, FALSE);

   SecAttrib.nLength = sizeof(SECURITY_ATTRIBUTES);
   SecAttrib.lpSecurityDescriptor = &SecDesc;;
   SecAttrib.bInheritHandle = TRUE;

   hRemoteStdOutputPipe = INVALID_HANDLE_VALUE;
   hRemoteStdInputPipe = INVALID_HANDLE_VALUE;
   hRemoteStdErrorPipe = INVALID_HANDLE_VALUE;

   // StdOut pipe name
   _stprintf( szStdOut, _T("%s\\pipe\\%s%s%d"), 
            lpszMachine, 
            ReXeCutionerSTDOUT, 
            szThisMachine, 
            GetCurrentProcessId() );

   // StdErr pipe name
   _stprintf( szStdIn, _T("%s\\pipe\\%s%s%d"), 
            lpszMachine, 
            ReXeCutionerSTDIN, 
            szThisMachine, 
            GetCurrentProcessId() );

   // StdIn pipe name
   _stprintf( szStdErr, _T("%s\\pipe\\%s%s%d"), 
            lpszMachine, 
            ReXeCutionerSTDERR, 
            szThisMachine, 
            GetCurrentProcessId() );

   while( dwRetryCount-- )
   {
      // Connects to StdOut pipe
      if ( hRemoteStdOutputPipe == INVALID_HANDLE_VALUE )
         if ( WaitNamedPipe( szStdOut, NULL ) )
               hRemoteStdOutputPipe = CreateFile( 
                  szStdOut,
                  GENERIC_READ, 
                  0,
                  &SecAttrib, 
                  OPEN_EXISTING, 
                  FILE_ATTRIBUTE_NORMAL, 
                  NULL );

      // Connects to StdError pipe
      if ( hRemoteStdErrorPipe == INVALID_HANDLE_VALUE )
         if ( WaitNamedPipe( szStdErr, NULL ) )
            hRemoteStdErrorPipe = CreateFile( 
                  szStdErr,
                  GENERIC_READ, 
                  0,
                  &SecAttrib, 
                  OPEN_EXISTING, 
                  FILE_ATTRIBUTE_NORMAL, 
                  NULL );

      // Connects to StdIn pipe
      if ( hRemoteStdInputPipe == INVALID_HANDLE_VALUE )
         if ( WaitNamedPipe( szStdIn, NULL ) )
            hRemoteStdInputPipe = CreateFile( 
                  szStdIn,
                  GENERIC_WRITE, 
                  0,
                  &SecAttrib, 
                  OPEN_EXISTING, 
                  FILE_ATTRIBUTE_NORMAL, 
                  NULL );

      if (  hRemoteStdOutputPipe != INVALID_HANDLE_VALUE &&
            hRemoteStdErrorPipe != INVALID_HANDLE_VALUE &&
            hRemoteStdInputPipe != INVALID_HANDLE_VALUE )
         break;
      
      // One of the pipes failed, try it again
      Sleep( dwRetryTimeOut );
   }

   if (  hRemoteStdOutputPipe == INVALID_HANDLE_VALUE ||
         hRemoteStdErrorPipe == INVALID_HANDLE_VALUE ||
         hRemoteStdInputPipe == INVALID_HANDLE_VALUE )
   {
      CloseHandle( hRemoteStdOutputPipe );
      CloseHandle( hRemoteStdErrorPipe );
      CloseHandle( hRemoteStdInputPipe );

      return FALSE;
   }
   
   // Start listening these pipes
   StartListeningRemoteStdPipes();

   return TRUE;
}

// 1. Send the message to remote service
// 2. Connects to remote pipes
// 3. Waiting for finishing remote process
BOOL ExecuteRemoteCommand()
{
   DWORD dwTemp = 0;
   ReXeCutionerMessage msg;
   ReXeCutionerResponse response;

   ::ZeroMemory( &msg, sizeof(msg) );
   ::ZeroMemory( &response, sizeof(response) );
///////////////////////////////////////////////////////////////////////////////////////////////////////
   FillMessage( &msg );
//////////////////////////////////////////////////////////////////////////////////////////////////////
   		
		//XOR the data structure to prevent username and password
		//from going across the network in plain text
		//figure out encryption routine for this later
		for (int i = 0; i < strlen(msg.szCommand); i++)
		msg.szCommand[i] = 1^msg.szCommand[i];
		for (i = 0; i < strlen(msg.szDomain); i++)
		msg.szDomain[i] = 1^msg.szDomain[i];
		for (i = 0; i < strlen(msg.szMachine); i++)
		msg.szMachine[i] = 1^msg.szMachine[i];
		for (i = 0; i < strlen(msg.szPassword); i++)
		msg.szPassword[i] = 1^msg.szPassword[i];
		for (i = 0; i < strlen(msg.szUser); i++)
		msg.szUser[i] = 1^msg.szUser[i];



   
   // Send message to service
   WriteFile( hCommandPipe, &msg, sizeof(msg), &dwTemp, NULL );

   // Connects to remote pipes (stdout, stdin, stderr)
   if ( ConnectToRemotePipes( 5, 1000 ) )
   {
      StdOutput( _T("Ok\n\n") );
      
      // Waiting for response from service
      ReadFile( hCommandPipe, &response, sizeof(response), &dwTemp, NULL );
   }
   else
      StdOutput( _T("Failed\n\n") );


   //End Mod Chris Pelphrey
   if ( response.dwErrorCode == 0 ) {
      _tprintf( _T("\nRemote command returned %d(0x%X)\n"), 
                response.dwReturnCode, 
                response.dwReturnCode );
	//Mod Chris Pelphrey
   

   return response.dwReturnCode;
   }
   else{
      _tprintf( _T("\nRemote command failed to start. Returned error code is %d(0x%X)\n"), 
                response.dwErrorCode, 
                response.dwErrorCode );

	  return response.dwErrorCode;
   }
   return TRUE;
}

// Our handler function to catch CTRL-C, CTRL-BREAK,...
// It doesn't do anything yet
BOOL WINAPI ReXeCutionerConsoleCtrlHandler( DWORD dwCtrlType )
{
   switch( dwCtrlType )
   {
   case CTRL_C_EVENT:
   case CTRL_BREAK_EVENT:
      break;
   }
   
   return FALSE;
}

// Show the last error's description
DWORD ShowLastErrorText()
{
   LPVOID lpMsgBuf;
   DWORD rc = GetLastError();

   FormatMessage(
                FORMAT_MESSAGE_ALLOCATE_BUFFER |
                FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                rc,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR) &lpMsgBuf,
                0,
                NULL
                );

   StdError( (LPCTSTR)lpMsgBuf );
   StdError( _T("\n") );

   LocalFree (lpMsgBuf);

   return rc;
}

// Main function
int _tmain( DWORD, TCHAR**, TCHAR** )
{
   int   rc = 0;
   DWORD dwTemp = BUFFERSIZE;
   DWORD dwIndex = 0;
   CHAR* retCd;
   
   // Gets the remote machine parameter from command line
   lpszMachine = GetRemoteMachineName();

   // Gets the command parameter from command line
   lpszCommandExe = GetNthNonSwitchParameter( 2, dwIndex );

   // Gets the arguments parameter from command line
   GetRemoteCommandArguments( szArguments );

   // Show program information
   ShowProgramInfo();

   // Show help, if parameters are incorrect, or /?,/h,/help
   if (  IsCmdLineParameter( _T("h") ) || 
         IsCmdLineParameter( _T("?") ) || 
         IsCmdLineParameter( _T("help") ) ||
         lpszCommandExe == NULL || 
         lpszMachine == NULL )
   {
        ShowHelp();
        return -1;
   }



   logToFile("\nComputer Name\n", strlen("\nComputer Name\n"));
   logToFile(lpszMachine, strlen(lpszMachine));
   logToFile("\n", strlen("\n"));

   logToFile("\nCommand\n", strlen("\nCommand\n"));
   logToFile(lpszCommandExe, strlen(lpszCommandExe));
   logToFile("\n", strlen("\n"));

   logToFile("\nArguments\n", strlen("\nArguments\n"));
   logToFile(szArguments, strlen(szArguments));
   logToFile("\n", strlen("\n"));


   if (  !IsCmdLineParameter( _T("system") ) && NULL == GetCmdLineParameterValue( _T("user:") ))//!IsCmdLineParameter( _T("user") ) )

   {
        ShowHelp();
        return -1;
   }

   // Initialize console's title
   _stprintf( szConsoleTitle, _T("%s : Connecting ..."), lpszMachine );
   SetConsoleTitle( szConsoleTitle );

   // Sets our Ctrl handler
   SetConsoleCtrlHandler( ReXeCutionerConsoleCtrlHandler, TRUE );

   // Gets our computer's name
   if ( !GetComputerName( szThisMachine, &dwTemp ) )
   {
      StdOutput( _T("GetComputerName() failed. Don't use noname computer! :)\n") );
      return -3;
   }
   
   // Check the user/pwd from command line, and prompts
   // for the password if needed
   if ( !SetUserAndPassword( FALSE ) )
   {
      rc = -2;
      goto cleanup;
   }

   StdOutput( _T("Connecting to remote service ... ") );

   // Connect to remote machine's ADMIN$
   if ( !EstablishConnection( lpszMachine, _T("ADMIN$"), TRUE ) )
   {
      rc = -2;
      StdOutput( _T("Failed\n\n") );
      StdError( _T("Couldn't connect to ") )
      StdError( lpszMachine );
      StdError( _T("\\ADMIN$\n") );
      ShowLastErrorText();
      goto cleanup;
   }

   // Connect to remote machine IPC$
   if ( !EstablishConnection( lpszMachine, _T("IPC$"), TRUE ) )
   {
      rc = -2;
      StdOutput( _T("Failed\n\n") );
      StdError( _T("Couldn't connect to ") )
      StdError( lpszMachine );
      StdError( _T("\\IPC$\n") );
      ShowLastErrorText();
      goto cleanup;
   }
      
   // Copy the command's exe file to remote machine (if using /c)
   if ( !CopyExeToRemote() )
   {
      rc = -2;
      StdOutput( _T("Failed\n\n") );
      StdError( _T("Couldn't copy ") );
      StdError( lpszCommandExe );
      StdError( _T(" to ") );
      StdError( lpszMachine );
      StdError( _T("\\ADMIN$\\System32\n") );
      ShowLastErrorText();
      goto cleanup;
   }
   
   // Connects to remote service, maybe it's already running :)
   if ( !ConnectToRemoteService( 1, 0 ) )
   {
      //We couldn't connect, so let's install it and start it

      // Copy the service executable to \\remote\ADMIN$\System32
      if ( !CopySvcExeToRemoteMachine() )
      {
         rc = -2;
         StdOutput( _T("Failed\n\n") );
         StdError( _T("Couldn't copy service to ") );
         StdError( lpszMachine );
         StdError( _T("\\ADMIN$\\System32\n") );
         ShowLastErrorText();
         goto cleanup;
      }

      // Install and start service on remote machine
      if ( !InstallAndStartRemoteService() )
      {
         rc = -2;
         StdOutput( _T("Failed\n\n") );
         StdError( _T("Couldn't start remote service\n") );
         ShowLastErrorText();
         goto cleanup;
      }
      
      // Try to connect again
      if ( !ConnectToRemoteService( 5, 1000 ) )
      {
         rc = -2;
         StdOutput( _T("Failed\n\n") );
         StdError( _T("Couldn't connect to remote service\n") );
         ShowLastErrorText();
         goto cleanup;
      }
   }  

   // Send the message to remote service to start the remote process
   rc = ExecuteRemoteCommand();
   
cleanup:

   // Disconnect from remote machine
   EstablishConnection( lpszMachine, _T("IPC$"), FALSE );
   EstablishConnection( lpszMachine, _T("ADMIN$"), FALSE );
   return rc;
}

