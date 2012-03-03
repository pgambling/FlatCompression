// Registry.h: Schnittstelle für die Klasse CRegistry.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_REGISTRY_H__008FD496_9EA1_11D4_A8C2_0050FC05DB87__INCLUDED_)
#define AFX_REGISTRY_H__008FD496_9EA1_11D4_A8C2_0050FC05DB87__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CRegistry  
{
public:
	typedef enum {
		REGISTRY_KEY_CREATE		= 0x01,
		REGISTRY_KEY_READONLY	= 0x02,
		REGISTRY_KEY_WRITEONLY	= 0x04,
		REGISTRY_KEY_READWRITE	= (REGISTRY_KEY_READONLY|REGISTRY_KEY_WRITEONLY)
	} tFlags;

	typedef enum {
		TYPE_UNKNOWN,
		TYPE_DWORD,
		TYPE_SZ,
		TYPE_BINARY
	} tKeyType;

public:
	CRegistry(HKEY hkey = 0, LPCTSTR szRegNode = 0, const int flags = REGISTRY_KEY_READWRITE|REGISTRY_KEY_CREATE);
	virtual ~CRegistry();

public:

	bool Open( HKEY hkey, LPCTSTR szRegNode,  const int = 0 );
	void Close();

	// inquire key type
	tKeyType GetKeyType( LPCTSTR szValue );

	// Read
	bool ReadDWORD(LPCTSTR szValue, DWORD*);
	bool ReadString(LPCTSTR szValue, BYTE* szBuf, DWORD*pSize);
	bool ReadData(LPCTSTR szValue, BYTE* szBuf, DWORD*pSize);

	// Write
	void WriteDWORD(LPCTSTR szValue, DWORD val);
	void WriteString(LPCTSTR szValue, BYTE* szBuf, DWORD size);
	void WriteData(LPCTSTR szValue, BYTE* szBuf, DWORD size);

	// Multi string helpers
	static LPCTSTR GetNextString( LPCTSTR szValueInput );

private:
	HKEY m_hKey;
};

#endif // !defined(AFX_REGISTRY_H__008FD496_9EA1_11D4_A8C2_0050FC05DB87__INCLUDED_)
