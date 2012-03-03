// Registry.cpp: Implementierung der Klasse CRegistry.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <tchar.h>
#include "Registry.h"
#include "debugtools.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Konstruktion/Destruktion
//////////////////////////////////////////////////////////////////////

CRegistry::CRegistry(HKEY hkey, LPCTSTR szRegKey, const int flags)
: m_hKey(0)
{
	if( szRegKey ) 
		Open(hkey, szRegKey, flags);
}

CRegistry::~CRegistry()
{
	Close();
}

bool CRegistry::Open(HKEY hkey, LPCTSTR szRegKey, const int flags)
{
	Close();

	if( flags & REGISTRY_KEY_CREATE ) {
		// create key here
		// not supported right now
		// open root key
		// parse szRegKey
		// open sub key
		// if not existing, create sub key
		// and so on...
		return false;
	} else {
		REGSAM regsam = 0;
		if( flags & REGISTRY_KEY_READONLY )
			regsam |= KEY_READ;
		if( flags & REGISTRY_KEY_WRITEONLY )
			regsam |= KEY_WRITE;
		
		// do not create, open registry node
		LONG res = ::RegOpenKeyEx(
			hkey,
			szRegKey,
			0,
			regsam,
			&m_hKey
			);
		if( res == ERROR_SUCCESS ) 
		{
			// everything went ok!
			return true;
		} else {
			//
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
			_OADS(_T("Registry key open failed!"));
			_OADS((LPCTSTR)lpMsgBuf);
//			MessageBox( NULL, (LPCTSTR)lpMsgBuf, "Error", MB_OK | MB_ICONINFORMATION );
			// Free the buffer.
			LocalFree( lpMsgBuf );
			m_hKey = 0;
			return false;
		}
	}
}

void CRegistry::Close()
{
	// close registry node
	if( m_hKey ) 
		RegCloseKey(m_hKey);
}

CRegistry::tKeyType CRegistry::GetKeyType(LPCTSTR szValueName)
{
	DWORD dwKeyType;

	if( m_hKey ) {
		if( RegQueryValueEx( m_hKey, szValueName, 0, &dwKeyType, NULL, NULL ) 
			== ERROR_SUCCESS ) 
		{
			// ok, went well
			switch(dwKeyType) {
			case REG_DWORD:
				return TYPE_DWORD;
			case REG_SZ:
				return TYPE_SZ;
			case REG_BINARY:
				return TYPE_BINARY;
			default:
				return TYPE_UNKNOWN;
			}
		} 
	}
	return TYPE_UNKNOWN;
}

bool CRegistry::ReadDWORD(LPCTSTR szValueName, DWORD *pVal)
{
	DWORD size = sizeof(DWORD), dw;
	BYTE buf[sizeof(DWORD)];

	if( m_hKey ) {
		if( RegQueryValueEx( m_hKey, szValueName, 0, &dw, buf, &size ) 
			== ERROR_SUCCESS ) 
		{
			// ok, went well
			ASSERT(dw==REG_DWORD);
			*pVal =  *(DWORD*)buf;
			return true;
		} 
	}
	return false;
}

bool  CRegistry::ReadString(LPCTSTR szValueName, BYTE* szBuf, DWORD* pSize)
{
	DWORD dw;
	if( m_hKey ) {
		if( RegQueryValueEx( m_hKey, szValueName, 0, &dw, szBuf, pSize ) 
			== ERROR_SUCCESS ) 
		{
			// ok, went well
			ASSERT(dw==REG_SZ||dw==REG_MULTI_SZ);
			return true;
		} 
	}
	return false;
}

bool CRegistry::ReadData(LPCTSTR szValueName, BYTE* szBuf, DWORD* pSize)
{
	DWORD dw;
	if( m_hKey ) {
		if( RegQueryValueEx( m_hKey, szValueName, 0, &dw, szBuf, pSize ) 
			== ERROR_SUCCESS ) 
		{
			// ok, went well
			ASSERT(dw==REG_BINARY);
			return true;
		} 
	}
	return false;
}

void CRegistry::WriteDWORD(LPCTSTR szValueName, DWORD val)
{
	DWORD size = sizeof(DWORD);
	BYTE buf[sizeof(DWORD)];

	*(DWORD*)buf = val;

	if( m_hKey ) {
		if( RegSetValueEx( m_hKey, szValueName, 0, REG_DWORD, buf, size ) 
			== ERROR_SUCCESS ) 
		{
			// ok, went well
			return;
		} 
	}
	return;
}

void CRegistry::WriteString(LPCTSTR szValueName, BYTE* szBuf, DWORD size)
{
	if( m_hKey ) {
		if( RegSetValueEx( m_hKey, szValueName, 0, REG_SZ, szBuf, size ) 
			== ERROR_SUCCESS ) 
		{
			// ok, went well
			return;
		} 
	}
	return;
}

void CRegistry::WriteData(LPCTSTR szValueName, BYTE* buf, DWORD size)
{
	if( m_hKey ) {
		if( RegSetValueEx( m_hKey, szValueName, 0, REG_BINARY, buf, size ) 
			== ERROR_SUCCESS ) 
		{
			// ok, went well
			return;
		} 
	}
	return;
}

//
// Helpers
//
LPCTSTR CRegistry::GetNextString( LPCTSTR szValueInput )
{
	// search end of current string
	while( *szValueInput != 0 )
		szValueInput++;
	// end found
	// goto start of next string
	szValueInput++;
	return szValueInput;
}
