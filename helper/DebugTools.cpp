#include "stdafx.h"
#include "DebugTools.h"
#include <tchar.h>
#include <stdio.h>
#include <stdarg.h>


#ifndef REMOVE_ODS

void OutputArgumentedDebugString(LPCTSTR szFormat, ... )
{
	va_list argList;
	TCHAR szHelper[c_dwMaxODSHelperBuffer];
	
	va_start(argList, szFormat);
	
//	wvsprintf(szHelper, szFormat, argList);
	_vsntprintf(szHelper, c_dwMaxODSHelperBuffer, szFormat, argList);
	
	OutputDebugString(szHelper);
	
	va_end(argList);
}

void MessageBoxOfLastError()
{
	LPVOID lpMsgBuf;
	FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM | 
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		NULL 
	);
	// Process any inserts in lpMsgBuf.
	// ...
	// Display the string.
	MessageBox( NULL, (LPCTSTR)lpMsgBuf, _T("Error"), MB_OK | MB_ICONINFORMATION );
	// Free the buffer.
	LocalFree( lpMsgBuf );
}

void OutputOfLastError()
{
	LPVOID lpMsgBuf;
	FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM | 
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		NULL 
	);
	// Process any inserts in lpMsgBuf.
	// ...
	// Display the string.
	OutputArgumentedDebugString( _T("Error occured: %s"), lpMsgBuf );
	// Free the buffer.
	LocalFree( lpMsgBuf );
}




#endif // REMOVE_ODS
