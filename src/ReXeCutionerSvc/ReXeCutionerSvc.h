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
// ReXeCutionerSvc.h
//
// History:
//
//    3/27/2001      Initial version
//
///////////////////////////////////////////////////////////////////////////////////

#ifndef ReXeCutionerSVC_H_INCLUDED
#define ReXeCutionerSVC_H_INCLUDED

extern HANDLE hStopServiceEvent;

void _ServiceMain(void*);
void DeleteSvc();

#endif