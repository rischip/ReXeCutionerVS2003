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
// ReXeCutioner.h
//
// History:
//
//    3/27/2001      Initial version
//
///////////////////////////////////////////////////////////////////////////////////

#ifndef ReXeCutioner_H_INCLUDED
#define ReXeCutioner_H_INCLUDED

#define SERVICENAME        _T("ReXeCutionerSvc")
#define LONGSERVICENAME    _T("ReXeCutioner Service")

#define ReXeCutionerSVCEXE         _T("ReXeCutionerSvc.exe")

#define ReXeCutionerCOMM           _T("ReXeCutioner_communication")
#define ReXeCutionerSTDOUT         _T("ReXeCutioner_stdout")
#define ReXeCutionerSTDIN          _T("ReXeCutioner_stdin")
#define ReXeCutionerSTDERR         _T("ReXeCutioner_stderr")

#define StdOutput(x)       { _ftprintf( stdout, _T("%s"), x); fflush(stdout); }
#define StdError(x)        { _ftprintf( stderr, _T("%s"), x); fflush(stderr); }

class ReXeCutionerMessage
{
public:
   TCHAR szCommand[0x1000];
   TCHAR szWorkingDir[_MAX_PATH];
   DWORD dwPriority;
   DWORD dwProcessId;
   TCHAR szMachine[_MAX_PATH];
   BOOL  bNoWait;
   TCHAR szDomain[_MAX_PATH];
   TCHAR szUser[_MAX_PATH];
   TCHAR szPassword[_MAX_PATH];
   BOOL  bSystem;
   BOOL  bInteractive;


};

class ReXeCutionerResponse
{
public:
   DWORD dwErrorCode;
   DWORD dwReturnCode;
};

#endif