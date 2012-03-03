#ifndef __DEBUG_TOOLS_INCLUDED__
#define __DEBUG_TOOLS_INCLUDED__


////////////////////////////////////////////////////////////////////////////
// constants

const DWORD c_dwMaxODSHelperBuffer	=  MAX_PATH * 4;


///////////////////////////////////////////////////////////////////////////
// samples
//
//	Output a text string: _ODS(_T("This is a sample\r\n"));
//  Output a text string with one argument: _OADS1(_T("This is a sample with arg %d\r\n"), arg1);
//  ...
//	Output a text string with many arguments: _OADS(_T("%02X%02X%02X%02X%02X%02X\r\n"), addr[0],
//  addr[1], addr[2], addr[3], addr[4], addr[5]);
//
//	Quikview a UINT variable:  _WATCHUINT(m_BindingData.Lines[0].uiAnswerId);
//  Quikview a string variable:  _WATCHTEXT(m_VisuData.szFrage);
//  Quikview hex representation of int variable: _WATCHHEX(pData->nMyVar); 


////////////////////////////////////////////////////////////////////////////
// Debug helpers

#ifndef REMOVE_ODS

#define _ODS(szString)							OutputDebugString(szString)
#define _OADS1(szFormat, p1)					OutputArgumentedDebugString(szFormat, p1)
#define _OADS2(szFormat, p1, p2)				OutputArgumentedDebugString(szFormat, p1, p2)
#define _OADS3(szFormat, p1, p2, p3)			OutputArgumentedDebugString(szFormat, p1, p2, p3)
#define _OADS									OutputArgumentedDebugString
//#define _OERR									MessageBoxOfLastError
#define _OERR									OutputOfLastError

void OutputArgumentedDebugString(LPCTSTR szFormat, ... );
void MessageBoxOfLastError();
void OutputOfLastError();

#else

#define _ODS(szString)							((void)0)
#define _OADS1(szFormat, p1)					((void)0)
#define _OADS2(szFormat, p1, p2)				((void)0)
#define _OADS3(szFormat, p1, p2, p3)			((void)0)
#define _OADS									1 ? (void)0 : OutputArgumentedDebugString
#define _OERR									1 ? (void)0 : MessageBoxOfLastError

inline void OutputArgumentedDebugString(LPCTSTR szFormat, ... ) {}
inline void MessageBoxOfLastError() {}

#endif // REMOVE_ODS

#define _WATCH(x, y)							_OADS3(_T("%s(%d) "#x" = %"#y"\r\n"), 1 + _tcsrchr(__FILE__, _T('\\')), __LINE__, ##x )
#define _WATCHHEX(x)							_OADS3(_T("%s(%d) "#x" = 0x%X\r\n"), 1 + _tcsrchr(__FILE__, _T('\\')), __LINE__, ##x )
#define _WATCHINT(x)							_WATCH(x, d)
#define _WATCHUINT(x)							_WATCH(x, u)
#define _WATCHLONG(x)							_WATCH(x, ld)
#define _WATCHULONG(x)							_WATCH(x, lu)
#define _WATCHTEXT(x)							_WATCH(x, s)

///////////////////////////////////////////////////////////////////////////////////////////////////
// simplified use of pragma message ( thanks to Jeffrey Richter )

#define dtSTR1(x)								#x
#define dtSTR(x)								dtSTR1(x)
#define dtMSG(content)							message(__FILE__ "(" dtSTR(__LINE__) "):" #content)

#endif // __DEBUG_TOOLS_INCLUDED__