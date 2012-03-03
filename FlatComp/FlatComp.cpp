//
//	Project: FlatCompression
//
//	ISAPI Filter for gzip/deflate compression
//
//	------------------------------------------
//
//	FlatComp.cpp
//
//	Main ISAPI filter file
//	
//	Project created/started by Uli Margull / 1mal1 Software GmbH
//	Version	Date		Who			What
//	1.03	2002-06-19	U.Margull	Preparing Open Source Release:
//									- removing 1mal1 logging, adding trace


#pragma warning( disable : 4786)  // Disable warning messages C4786

#include "stdafx.h"
#include "build.h"
#include <string.h>
#include <tchar.h>
#include <time.h>
#include <algorithm>
#include <vector>
#include <string>
#include "Dechunkifier.h"
#include "FlatComp.h"
#include "..\\helper\\trace.h"
#include "..\\helper\\registry.h"
extern "C" {
#include "..\\zlib\\zlib.h"
#include "..\\zlib\\zutil.h"
}

#define REGISTRY_PATH			"SOFTWARE\\FlatCompression"

#define FILTER_DESCRIPTION		"FlatCompression"
#define FILTER_MAJOR_VERSION	1
#define FILTER_MINOR_VERSION	20
#ifdef _DEBUG
#define FILTER_VERSION_TYPE		"(debug)"
#else
#define FILTER_VERSION_TYPE		"(release)"
#endif

#define HEADER_BUFFER_SPARE		16

#ifdef FLATCOMP_DEBUG
#define ERROR_COUNT		++pReq->dwErrorCount
#else
#define ERROR_COUNT		0
#endif

static DWORD glErrorBase = 0;
static const int MAX_BUF_SIZE = 1024;
static char *strEncodingName[] = {
	"gzip",
	"deflate",
	"" 
};

typedef struct SReqContextFlatComp 
{
	/* typedefs */
	typedef enum {
		GZIP,
		DEFLATE,
		NONE
	} TypeEncoding;
	/* public variables */
	HTTP_FILTER_RAW_DATA rawHeader;
	HTTP_FILTER_RAW_DATA rawHtmlData;
	DWORD nReqCount;
	/* data transported */
	bool bDoEncoding;
	TypeEncoding tEncoding;
	DWORD dwOriginalSize;
	DWORD dwCompressedSize;
	CDechunkifier * poDechunkifier;
#ifdef FLATCOMP_DEBUG
	DWORD dwErrorCount;
	char strUrl[1024+1];
#endif
public:
	// constructor
	void Construct() {
		dwOriginalSize = 0;
		nReqCount = 0;
		bDoEncoding = false;
		tEncoding = NONE;
		poDechunkifier = NULL;
#ifdef FLATCOMP_DEBUG
		// calculate new error base
		glErrorBase += 1000;
		glErrorBase %= 1000000;
		dwErrorCount = glErrorBase;
		strUrl[0] = 0;
#endif
	}
} REQ_CONTEXT, *PREQ_CONTEXT;

typedef struct
{
	const char* cptr;		// pointer to string
	DWORD size;				// size of string
} structStringPointerType;

static DWORD g_dwFlags = 0;
static DWORD g_dwLogSize = 0;
static DWORD g_dwMinSize = 0;
static DWORD g_dwMaxSize = 0;
static DWORD dwBufMimeSize;
static const DWORD BUF_MIME_MAX = 2048;
static BYTE glBufMime[ BUF_MIME_MAX ];
static const int ARR_MIME_MAX = 5;
static DWORD dwArrMimeSize;
static structStringPointerType glArrMime[ARR_MIME_MAX];
static const int STR_MAX = 1024;
static char g_strInfoPath[STR_MAX+128] = "/Flatcompression";

// 
// helper functions
//
inline void HFRDCopy( PHTTP_FILTER_RAW_DATA ptrDest, PHTTP_FILTER_RAW_DATA ptrSrc )
{
	ptrDest->cbInBuffer = ptrSrc->cbInBuffer;
	ptrDest->cbInData = ptrSrc->cbInData;
	ptrDest->pvInData = ptrSrc->pvInData;
}

inline bool FormatTimeNow( char* buf, DWORD* ptrBufSize )
{
	time_t ltime;
	struct tm * gmt;

	if( *ptrBufSize < 32 )
	{
		return false;
	}
	time( &ltime );
	gmt = gmtime( &ltime );
	// now fill time string according to RFC 1123
	strftime( buf, *ptrBufSize, "%a, %d %b %Y %H:%M:%S GMT", gmt );
	*ptrBufSize = strlen( buf );
	return true;
}


//
// InitFilter
//
BOOL InitFilter()
{
	// start logging
	const int bufSize = 1024;
	BYTE bufFilename[bufSize];
	DWORD dwBufSize = bufSize;
	CRegistry myReg( HKEY_LOCAL_MACHINE, REGISTRY_PATH, CRegistry::REGISTRY_KEY_READONLY );
	if( !myReg.ReadString( _T("LogFile"), bufFilename, &dwBufSize ) )
		strncpy( (char*)bufFilename, "C:\\FlatComp.log", bufSize );
	if( !myReg.ReadDWORD( _T("LogSize"), &g_dwLogSize ) )
		g_dwLogSize = 8192;
	if( !myReg.ReadDWORD( _T("Flags"), &g_dwFlags ) )
		g_dwFlags = 0;
	if( !myReg.ReadDWORD( _T("MinSize"), &g_dwMinSize ) )
		g_dwMinSize = 0;
	if( !myReg.ReadDWORD( _T("MaxSize"), &g_dwMaxSize ) )
		g_dwMaxSize = 0;

	TRACE_INITIALIZE(g_dwLogSize, (char*)bufFilename );

	_sntprintf( (char*)bufFilename, bufSize, FILTER_DESCRIPTION " R%d.%02d." BUILD " " FILTER_VERSION_TYPE " initialized", FILTER_MAJOR_VERSION, FILTER_MINOR_VERSION );
	TRACE( TRACE_MESSAGE, (char*)bufFilename, sizeof(REQ_CONTEXT) );
//	TRACE( TRACE_MESSAGE, "Re-Initialize using the following URL on the server: /FlatCompression/InitFilter", sizeof(REQ_CONTEXT) );


	// temp buffer for Unicode string
	dwBufMimeSize = BUF_MIME_MAX;
	if( !myReg.ReadString( _T("mime"), glBufMime, &dwBufMimeSize ) ) 
	{
		// build a default multi string
//		TRACE( TRACE_MESSAGE, "FlatCompression/InitFilter: using default mime settings", 0 );
//		char str[] = "text/,application/x-javascript,,";  // IE6 does not work with compressed javascript
		char str[] = "text/,,";
		int len = strlen(str);
		char * cp = str;
		// now replace all ',' by 0
		do {
			cp = strchr( cp, ',' );
			if( cp ) *cp = 0;
		} while( cp );
		// copy default multi string
		memcpy( glBufMime, str, len );
		// done!
	}
	// construct path to info page from passwor setting
	DWORD dw = bufSize;
	bool bRes = myReg.ReadString(_T("password"), bufFilename, &dw );
	if( bRes && strlen((char*)bufFilename)>0 ) {
		strcpy( g_strInfoPath, "/FlatCompression/" );
		strncat( g_strInfoPath, (char*)bufFilename, STR_MAX );
	} else {
		strcpy( g_strInfoPath, "/FlatCompression" );
	}

	// build up mime string array
	const char* cptr = (char*)glBufMime;
	dwArrMimeSize = 0;
	do {
		glArrMime[dwArrMimeSize].cptr = cptr;
		glArrMime[dwArrMimeSize].size = strlen(cptr);
//		_sntprintf( (char*)bufFilename, bufSize, "FlatCompression: Using Mime Type: %s", cptr );
//		TRACE( TRACE_MESSAGE, (char*)bufFilename, 0 );	
		dwArrMimeSize++;
		if( dwArrMimeSize>=ARR_MIME_MAX )
		{
			TRACE( TRACE_ERROR_SOFT, "Maximum size of mime types exceeded, cutting to n=", dwArrMimeSize );	
			break;
		}
		cptr = CRegistry::GetNextString( cptr );
	} while( *cptr );

	return TRUE;
}

//
// ConstructPage
//
void ConstructPage(std::string &html)
{
	// only one type support right now

	static char response_begin[] = "HTTP/1.0 200 OK\nCache-Control: no-cache\nContent-Type: text/html\nExpires: Tue, 20 Aug 1996 14:25:27 GMT\n\n\
<html><head><title>FlatCompression Info Page</title></head>\
<body bgcolor=\"#2986B5\"><h1>Flatcompression Info Page</h1>\
<p>More about <a href=\"http://www.flatcompression.org\">FlatCompression</a></p>\
<p>Need some help? Heres a link to the <a href=\"http://www.flatcompression.org/doc/FlatCompressionDocumentation.html\">documentation</a></p>\
";
	static char response_end[] = "</body></html>";

	// create large buffer
	int nlines = 512;
	int nsize = 256 * nlines;
	char* buf = new char[nsize];

	html = "";
	html.append( response_begin );
	// Reload filter settings
	html.append( "<p>Changed filter setting in the registry? <a href=\"" );
	html.append( g_strInfoPath );
	html.append( "/InitFilter\">re-initialize the filter settings</a></p>" );

	html.append( "<h2>Settings</h2>" );
	// magic Url
	html.append( "Magic Url: <b>");
	html.append( g_strInfoPath );
	html.append( "</b><br>" );
	// mime types
	html.append( "Mime Types: <b>" );
	int n = dwArrMimeSize;
	while( n > 0 ) {
		--n;
		html.append( glArrMime[n].cptr );
		if( n==0 ) 
			break;
		html.append( ", " );
	}
	// logfile size
	html.append( "</b><br>Logfile size: <b>");
	_itoa( g_dwLogSize, buf, 10 );
	html.append( buf );
	html.append( "</b>" );
	// minimum size
	html.append( "</b><br>Minimum response size (zero if not used): <b>");
	_itoa( g_dwMinSize, buf, 10 );
	html.append( buf );
	html.append( "</b>" );
	// maximum size
	html.append( "</b><br>Maximum response size (zero if not used): <b>");
	_itoa( g_dwMaxSize, buf, 10 );
	html.append( buf );
	html.append( "</b>" );
	// flags
	html.append( "<br>Log compressed files: " );
	if( g_dwFlags & 1 ) 
		html.append( "<b>enabled</b><br>" );
	else
		html.append( "<b>disabled</b><br>" );


	html.append( "<h2>Log</h2><table border=1>" );
	html.append( trace_html_header(0) );
	trace_html_format( buf, nsize, nlines, "" );
	html.append( buf );
	html.append( "</table>" );
	html.append( response_end );

	// delete buffer
	delete[]buf;
}

//
// ConstructRedirectionPage
//
// this page redirects us to the Flatcompression Info Page
//
void ConstructRedirectionPage(char* strMagicURL, std::string &html)
{
	// only one type support right now

	static char response1[] = "HTTP/1.0 301 MOVED\nLocation: ";
	static char response2[] = "\n\n";

	html = response1;
	html.append( strMagicURL );
	html.append( response2 );
}

void DisableFurtherNotificationsForThisRequest(HTTP_FILTER_CONTEXT* pFC)
{
	pFC->ServerSupportFunction( pFC, SF_REQ_DISABLE_NOTIFICATIONS, 0, SF_NOTIFY_SEND_RESPONSE | SF_NOTIFY_SEND_RAW_DATA | SF_NOTIFY_END_OF_REQUEST,0 );
}

//
// Select the notifications
//
BOOL WINAPI GetFilterVersion(HTTP_FILTER_VERSION *pVersion)
{
	// this is only done once!
	InitFilter();

	// Set version
	pVersion->dwFilterVersion = HTTP_FILTER_REVISION;
	
	// Provide a short description here
	lstrcpyn( pVersion->lpszFilterDesc, FILTER_DESCRIPTION,
		SF_MAX_FILTER_DESC_LEN);

	// Set the events we want to be notified about and the
	// notification priority, SF_NOTIFY_ORDER_LOW is used so we
	// don't get in the way of ASP ??
	pVersion->dwFlags = (SF_NOTIFY_ORDER_HIGH |
						 SF_NOTIFY_PREPROC_HEADERS |
						 SF_NOTIFY_SEND_RESPONSE |
						 SF_NOTIFY_SEND_RAW_DATA |
						 SF_NOTIFY_END_OF_REQUEST);

	// done
	return TRUE;
}


//
// Handle SF_NOTIFY_PREPROC_HEADERS event
//
DWORD HandleEvent_PreprocHeaders( HTTP_FILTER_CONTEXT* pFC, 
								 PHTTP_FILTER_PREPROC_HEADERS pPreprocData )
{
	DWORD dwRet = SF_STATUS_REQ_NEXT_NOTIFICATION;

	// create new context
	PREQ_CONTEXT pReq;
	pFC->pFilterContext = pReq = (PREQ_CONTEXT)pFC->AllocMem( pFC, sizeof(REQ_CONTEXT), 0);
	if( !pReq ) {
		TRACE( TRACE_ERROR_SOFT, "AllocMem() failed!", 0 );
		return dwRet;
	}
	pReq->Construct();

#ifdef FLATCOMP_DEBUG
	{
		DWORD len = 1023;
		pPreprocData->GetHeader( pFC, "URL", pReq->strUrl, &len );
	}

#endif

	// allocate buffer on stack
	const int BUF_SIZE = 1024;
	char bufHeader[BUF_SIZE+1];
	DWORD dwBufSize = BUF_SIZE;
	if( pPreprocData->GetHeader(pFC, "URL", bufHeader, &dwBufSize ) )
	{
		int l1 = strlen(g_strInfoPath);
		int l2 = strlen(bufHeader);
		// supplied path must be at least as long as the info path
		// and it must start equivalent
		if( (l1<=l2) && (strnicmp( bufHeader, g_strInfoPath, l1 )==0) )
		{
			// init?
			int off = strlen( g_strInfoPath );
			std::string html;
			if( stricmp( bufHeader+off, "/InitFilter")==0 
				|| stricmp( bufHeader+off, "/init.htm")==0 
				|| stricmp( bufHeader+off, "/init.html")==0 )
			{
				InitFilter();
				ConstructRedirectionPage(g_strInfoPath, html);
			} else {
				// construct info page
				ConstructPage( html );
			}
			if( ! html.empty() ) {
				// output page
				DWORD nsize = html.size();
				pFC->WriteClient( pFC, (void*)html.c_str(), &nsize, 0);
			}

			return SF_STATUS_REQ_FINISHED;
		}
	}


	dwBufSize = BUF_SIZE;
	if( pPreprocData->GetHeader(pFC, "Accept-Encoding:", bufHeader, &dwBufSize ) )
	{

		if( dwBufSize==BUF_SIZE ) {
			TRACE( TRACE_ERROR_SOFT, bufHeader, ERROR_COUNT );
			TRACE( TRACE_ERROR_SOFT, "Header buffer overflow", BUF_SIZE );
		}

		int i = 0;
		while( i != SReqContextFlatComp::NONE ) {
			// check whether this encoding is allowed
			if( strstr( bufHeader, strEncodingName[i])!=0 ) {
				// gzip found
				pReq->bDoEncoding = true;
				pReq->tEncoding = static_cast<SReqContextFlatComp::TypeEncoding>(i);
				break;
			}
			i++;
		}

	}

	if( ! pReq->bDoEncoding ) 
	{
		DisableFurtherNotificationsForThisRequest(pFC);
	}

	return dwRet;
}

//
// Handle SF_NOTIFY_SEND_RESPONSE event
//
DWORD HandleEvent_SendResponse( HTTP_FILTER_CONTEXT* pFC, 
								 PHTTP_FILTER_PREPROC_HEADERS pHeader )
{
	DWORD dwRet = SF_STATUS_REQ_NEXT_NOTIFICATION;
	PREQ_CONTEXT pReq = (PREQ_CONTEXT)pFC->pFilterContext;
	if( !pReq ) {
		TRACE( TRACE_ERROR_SOFT, "No context available!", 0 );
		return dwRet;
	}

	// allocate buffer on stack
	const int bufSize = 1024;
	char buf[bufSize];
	DWORD dwBufSize = bufSize;

	// check http response code
	switch(pHeader->HttpStatus) 
	{
	case 100:
		// continue
		pReq->bDoEncoding = false;
		break;
	case 200:
		// Check if the response already contains a content encoding, and if
		// so prevent any further processing.
		if( pHeader->GetHeader(pFC, "Content-Encoding:", buf, &dwBufSize) ) {
			pReq->bDoEncoding = false;
		}

		if( pHeader->GetHeader(pFC, "Content-Type:", buf, &dwBufSize) ) {
			// check for mime type to compress
			// for larger mime type fields, one should use a binary search
			// which could be faster. But then comparisons like "text/" are 
			// difficult (an appropiate Predicate is needed)
			bool bDoIt = false;
			unsigned int i = 0;
			while( i<dwArrMimeSize ) {
				if( strncmp( buf, glArrMime[i].cptr, glArrMime[i].size ) == 0 )
				{
					// ok, found match
					bDoIt = true;
					break;
				}
				i++;
			}

			if( bDoIt )
				pReq->bDoEncoding = true;
			else {
				pReq->bDoEncoding = false;
			}
		}

		if( pReq->bDoEncoding ) {
			dwBufSize = bufSize;
			if( pHeader->GetHeader(pFC, "Flatcompression-control:", buf, &dwBufSize) ) {
				// no-compression set for this response?
				if( strnicmp( buf, "no-compress", strlen("no-compress") )==0 )
					pReq->bDoEncoding = false;
			}
		}

		if( pReq->bDoEncoding ) {
			// check for content-length
			if( pHeader->GetHeader( pFC, "Content-Length:", buf, &dwBufSize) ) {
				// extract content length: this will be handled in the HandleEvent_SendRawData function
				// check minimum size
				unsigned size = atoi( buf );
				if( (g_dwMinSize>0) && (size<g_dwMinSize) ) 
					// no compression if smaller than minimum size
					pReq->bDoEncoding = false;
				if( (g_dwMaxSize>0) && (size>g_dwMaxSize) ) 
					// no compression if larger than maximum size
					pReq->bDoEncoding = false;

			} 
			else if(pHeader->GetHeader( pFC, "Transfer-Encoding:", buf, &dwBufSize)) // This is a chunked response
			{
				// Remove the Transfer-Encoding and add Content-Length
				pHeader->SetHeader(pFC, "Transfer-Encoding:", "\0");
				pHeader->AddHeader(pFC, "Content-Length:", "0");
				pReq->poDechunkifier = new CDechunkifier(pFC);
			}
		}

		// skip all further notifications if no compression 
		// is used
		if( ! pReq->bDoEncoding ) {
			// disable further notifications for this request
			DisableFurtherNotificationsForThisRequest(pFC);
		}

		break;
	default:
		// we encode only if status is OK
		pReq->bDoEncoding = false; 
		DisableFurtherNotificationsForThisRequest(pFC);
		break;
	} // eo switch

	if( pReq->bDoEncoding ) {
		// add the encoding header
		pHeader->SetHeader( pFC, "Content-Encoding:", strEncodingName[pReq->tEncoding] );
	} 

	return dwRet;
}

void * 	memstr( char*pmem, DWORD pmem_size, char*pin, char*cmp, DWORD cmp_size )
{
	do {
		// search first char
		pin = (char*)memchr((char*)pin, *cmp, pmem_size-(pin-pmem)-cmp_size+1 );
		if( !pin )
			return pin;
		if( strncmp(pin,cmp,cmp_size)==0 )
			return pin;
		pin++;
	} while( pin<pmem + pmem_size );
	return 0;
}

typedef std::vector<char*> cpVector;

void FindLinks( char * pInData, DWORD dwInData, 
			    char* cmp, DWORD cmp_size, 
				cpVector& cp_vec )
{
	char*pin = pInData;

	while(pin) {
		// we scan for href="
//		pin = strstr( pin, "href=\"" );
		pin = (char*)memstr( pInData, dwInData, (char*)pin, cmp, cmp_size );
		if( !pin ) 
			return;
		pin += cmp_size;
		cp_vec.push_back( pin );
	}

	return;
}


char* FindContentLengthHeader( char*pInBuffer, DWORD size )
{
	char * pin = (char*)pInBuffer;
	char token[] = "Content-Length";
	DWORD tokSize = strlen(token);
	while(1) {
		// search for delimiter ':'
		pin = (char*)memchr( (void*)pin, ':', (char*)pInBuffer + size - pin );
		// end reached ?
		if( !pin )
			// nothing found
			return 0;
		// correct string found ?
		if( ((DWORD)(pin - pInBuffer)>tokSize) 
			&& (strnicmp( pin - tokSize, token, tokSize ) == 0) ) 
		{
			return ++pin;
		}
		pin++;
	}
	// not reached 
}

// allocator functions
voidpf myAlloc( voidpf opaque, uInt items, uInt size )
{
	// get the context
	PHTTP_FILTER_CONTEXT pFC = static_cast<PHTTP_FILTER_CONTEXT>(opaque);
	DWORD dwSize = items*size;
	voidpf ptr = pFC->AllocMem( pFC, dwSize, 0 );
	if( !ptr )
	{
		TRACE( TRACE_ERROR_SOFT, "myAlloc() failed!", 0 );
	}
	return ptr;
}

void myFree( voidpf opaque, voidpf address )
{
	if( opaque ) opaque = 0; // make compiler happy
	if( address ) address = 0; // dito
	return;
}

enum {
	MYDEFLATE_OK = 0,
	MYDEFLATE_ERR_NOT_ENOUGH_OUTPUT = 1,
	MYDEFLATE_ERR_COULD_NOT_INITIALIZE = 2
};

DWORD MyDeflate( 
	Bytef* dest,				// destination buffer
	DWORD* pdwTotalOut,			// size of destination buffer
	Bytef* src,					// data source 
	DWORD dwTotalIn				// size of data source
	)
{
	// here comes the compression algorithm
	const int level = Z_DEFAULT_COMPRESSION;
	const int strategy = Z_DEFAULT_STRATEGY; /* compression strategy */

	// check if buffer is large enough
	// calculate max. possible input from out buffer
	DWORD dwMaxIn = *pdwTotalOut;
	// minus 12
	dwMaxIn -= 12;
	// minus 0.1 %: /256 is approx. 0.25 %, minus 1 to be sure
	dwMaxIn -= (dwMaxIn >> 8) + 1;
	if( dwMaxIn < dwTotalIn )
	{
		TRACE( TRACE_ERROR_SOFT, "MyDeflate(): not enough output room", 0 );
		return MYDEFLATE_ERR_NOT_ENOUGH_OUTPUT;
	}

	// do the compression thing
	z_stream c_stream;
	// clear data
	memset(&c_stream, 0, sizeof(c_stream) );
	// initialize memory allocation routines
/*	c_stream.zalloc = myAlloc;
	c_stream.zfree = myFree;
	c_stream.opaque = (voidpf)pFC;*/
	// use standard allocation scheme
	// all zero already!

	int err;

	// initialize compression routine
//	err = deflateInit( &c_stream, Z_DEFAULT_COMPRESSION );
	// we use this function, since we have to turn off the zlib header
	err = deflateInit2( &c_stream, level,
                           Z_DEFLATED, -MAX_WBITS, DEF_MEM_LEVEL, strategy);
	if( err!=Z_OK ) {
		// error, do nothing
		TRACE( TRACE_ERROR_SOFT, "deflateInit2() faild", err );
		return MYDEFLATE_ERR_COULD_NOT_INITIALIZE;
	}

	// setup the stream
	c_stream.next_in = src;
	c_stream.next_out = dest;
	c_stream.avail_in = dwTotalIn;
	c_stream.avail_out = *pdwTotalOut;

	// do compression
	err = deflate( &c_stream, Z_FINISH );
	if( (err!= Z_STREAM_END) ) 
	{
		// error, do nothing
		TRACE( TRACE_ERROR_SOFT, "could not deflate correctly, error code=", err );
	}
	err = deflateEnd(&c_stream);
	if( err!=Z_OK ) {
		// error, do nothing
		TRACE( TRACE_ERROR_SOFT, "deflateEnd() faild", err );
	}

	// store number of bytes we did output
	*pdwTotalOut = c_stream.total_out;

	return MYDEFLATE_OK;
}

//
// DeflateData
//
// Does the work of compression
//
DWORD DeflateData( HTTP_FILTER_CONTEXT* pFC, 
				    PHTTP_FILTER_RAW_DATA pRawData )
{
	// create new buffer, same size as original +12 byte + 0.1 % 
	// see zlib description
	DWORD dwSize = pRawData->cbInData;
	// add 12 bytes
	dwSize += 12;
	// add at least 0.1 %
	dwSize += (dwSize>>8) + 1;

	// create new buffer
	HTTP_FILTER_RAW_DATA sRaw;
	// clear memory
	memset( &sRaw, 0, sizeof(sRaw) );
	// allocate new buffer
	sRaw.pvInData = pFC->AllocMem( pFC, dwSize, 0 );
	if( sRaw.pvInData == 0 )
	{
		// Alloc Failed
		TRACE( TRACE_ERROR_SOFT, "AllocMem() failed!", 0 );
		return 1;
	}
	// set buffer size
	sRaw.cbInBuffer = dwSize;

	DWORD dwResult = MyDeflate( (Bytef*)sRaw.pvInData, &dwSize, (Bytef*)pRawData->pvInData, pRawData->cbInData );
	if( dwResult != MYDEFLATE_OK ) 
	{
		TRACE( TRACE_ERROR_SOFT, "MyDeflate() faild", dwResult );
		return 1;
	}

	// copy compressed data to data structure
	sRaw.cbInData = dwSize;
	HFRDCopy( pRawData, &sRaw );
	return 0;
}


static unsigned char glGzipHeader[] = {
	0x1F, 0x8B, 0x08, 0,	// Magic number 1, magic number 2, method=deflate, flags=none
	0, 0, 0, 0,				// modification date = none
	2, 0					// fastest, OS=Win32
};

static const DWORD GZIP_HEADER_SIZE = 10;
static const DWORD GZIP_FOOTER_SIZE = 8;
static const DWORD GZIP_ADDITIONAL_SIZE = GZIP_HEADER_SIZE + GZIP_FOOTER_SIZE;

//
// GzipData
//
// Does the work of compression
//
DWORD GzipData( HTTP_FILTER_CONTEXT* pFC, 
				    PHTTP_FILTER_RAW_DATA pRawData )
{

	// create new buffer, same size as original +12 byte + 0.1 % 
	// see zlib description
	DWORD dwSize = pRawData->cbInData;
	// add 12 bytes
	dwSize += 12;
	// add at least 0.1 %
	dwSize += (dwSize>>8) + 1;
	// plus gzip overhead (header 10 + footer 8 bytes)
	dwSize += GZIP_ADDITIONAL_SIZE;

	// some spare 
	dwSize += 1024;

	// create new buffer
	HTTP_FILTER_RAW_DATA sRaw;
	// clear memory
	memset( &sRaw, 0, sizeof(sRaw) );
	// allocate new buffer
	sRaw.pvInData = pFC->AllocMem( pFC, dwSize, 0 );
	if( sRaw.pvInData == 0 )
	{
		// Alloc Failed
		TRACE( TRACE_ERROR_SOFT, "AllocMem() failed!", 0 );
		return 1;
	}
	// set buffer size
	sRaw.cbInData = sRaw.cbInBuffer = dwSize;


	// fill the gzip header
	memcpy( sRaw.pvInData, glGzipHeader, GZIP_HEADER_SIZE );

	// do compression
	Bytef* ptr = (Bytef*)sRaw.pvInData;
	ptr += GZIP_HEADER_SIZE;
	dwSize -= GZIP_ADDITIONAL_SIZE;
	DWORD dwResult = MyDeflate( ptr, &dwSize, (Bytef*)pRawData->pvInData, pRawData->cbInData );
	if( dwResult != MYDEFLATE_OK ) 
	{
		TRACE( TRACE_ERROR_SOFT, "MyDeflate() faild", dwResult );
		return 1;
	}

	// add the gzip footer
	DWORD* dwPtrFooter = (DWORD*)((BYTE*)sRaw.pvInData + GZIP_HEADER_SIZE + dwSize);
	// store the crc32 checksum of original data
	uLong crc = crc32(0L, Z_NULL, 0);
	crc = crc32(crc, (BYTE*)pRawData->pvInData, pRawData->cbInData);
	*dwPtrFooter = crc;
	dwPtrFooter++;
	// store the length of the original data
	*dwPtrFooter = pRawData->cbInData;

	// store size of gzip data
	sRaw.cbInData = dwSize + GZIP_ADDITIONAL_SIZE;

	// copy compressed data to data structure
	HFRDCopy( pRawData, &sRaw );
	return 0;
}

//#define DEBUG_ZIP
#ifdef DEBUG_ZIP
static const int codstrSize = 28;
//static unsigned char codstr[] = { 4, 0xC1, 0x31, 0x0D, 0, 0, 8, 3, 0x41, 0x2b, 0xE0, 0xA6, 0x0E, 0x98, 0x3B, 0x74, \
//	0xFB, 0x84, 5, 0xFF, 0xE1, 0x4E, 0x86, 0xAD, 0x09, 0xD7, 0x3F, 0 };
static unsigned char codstr[] = { 0xF2, 0x48, 0xCD, 0xC9, 0xC9, 0x57, 0x28, 0xCF, 0x2F, 0xCA, 
0x49, 0x51, 0x04, 0x08, 0x30, 00 };

DWORD TestData( HTTP_FILTER_CONTEXT* pFC, 
				    PHTTP_FILTER_RAW_DATA pRawData,
					PREQ_CONTEXT pReq )
{
	pRawData->pvInData = pFC->AllocMem( pFC, codstrSize, 0 ); 
	pRawData->cbInData = pRawData->cbInBuffer = codstrSize;
	memcpy( pRawData->pvInData, (void*)codstr, codstrSize );
	return 0; 
		
}

//
//
// return a binary code that was provided from somewhere else
//
DWORD TestGzipData( HTTP_FILTER_CONTEXT* pFC, 
				    PHTTP_FILTER_RAW_DATA pRawData,
					PREQ_CONTEXT pReq )
{
	DWORD dwSize = codstrSize + 18;
	pRawData->pvInData = pFC->AllocMem( pFC, dwSize, 0 ); 
	pRawData->cbInData = pRawData->cbInBuffer = dwSize;
	unsigned char*cptr = (unsigned char*)pRawData->pvInData;
	memset( cptr, 0, dwSize );
	cptr[0] = 0x1F;
	cptr[1] = 0x8B;
	cptr[2] = 8;
	cptr[8] = 2;		// yes!

	cptr += 10;
	memcpy( cptr, (void*)codstr, codstrSize );

	// Footer, fixed:
	cptr += codstrSize;

	cptr[0] = 0x74;
	cptr[1] = 0x52;
	cptr[2] = 0x54;
	cptr[3] = 0xBE;
	cptr[4] = 0x0B;


	return 0; 
		
}

#endif

DWORD DoCompression(SReqContextFlatComp::TypeEncoding mode,
					HTTP_FILTER_CONTEXT* pFC, 
				    PHTTP_FILTER_RAW_DATA pRawData )

{
	switch(mode) {
	case SReqContextFlatComp::GZIP:
		return GzipData( pFC, pRawData );
//		return TestGzipData2( pFC, pRawData, pReq );
	case SReqContextFlatComp::DEFLATE:
		return DeflateData( pFC, pRawData );
	default:
		return 1;
	}
}

//
// ReplaceHeader
//
DWORD ReplaceHeader(PHTTP_FILTER_RAW_DATA pRawHeader,
					DWORD dwContentLength )
{
	char * pin = (char*)pRawHeader->pvInData;

	pin = FindContentLengthHeader( pin, pRawHeader->cbInData );
	if( !pin ) 
		return 0;

	// token found
	char * pend = pin;
	// search end of line
	DWORD lsize = pRawHeader->cbInData - (pend-(char*)pRawHeader->pvInData);
	while( (*pend!='\n') && (*pend!='\r') && lsize ) {
		*pend = ' ';
		pend++;
		lsize--;
	}

	assert( pRawHeader->cbInBuffer >= pRawHeader->cbInData + HEADER_BUFFER_SPARE );
	assert(lsize);
	if( lsize ) {
		memmove( (char*)pend+HEADER_BUFFER_SPARE, (char*)pend, lsize );
	}
	memset( (char*)pend, ' ', HEADER_BUFFER_SPARE );

	// now print content length
//	_ultoa( dwContentLength, pin+1, 10 );
	char buf[16];
	_ultoa( dwContentLength, buf, 10 );
	int len = strlen(buf);
	memcpy( pin+1, buf, len );
	
	// adapt new header length
	pRawHeader->cbInData += HEADER_BUFFER_SPARE;

	// done!
	return 1;
}

//
// Handle SF_NOTIFY_PREPROC_HEADERS event
//
DWORD HandleEvent_SendRawData( HTTP_FILTER_CONTEXT* pFC, 
								 PHTTP_FILTER_RAW_DATA pRawData
								 )
{
	DWORD dwRet = SF_STATUS_REQ_NEXT_NOTIFICATION;
	PREQ_CONTEXT pReq = (PREQ_CONTEXT)pFC->pFilterContext;
	if( !pReq ) {
		TRACE( TRACE_ERROR_SOFT, "No context available!", 0 );
		return dwRet;
	}

	if( !pReq->bDoEncoding ) { return dwRet; }

	// count the steps 
	pReq->nReqCount++;

	if( pReq->nReqCount==1 ) {
		// copy header data
		pReq->rawHeader.cbInData = pRawData->cbInData;
		pReq->rawHeader.cbInBuffer = pRawData->cbInData+HEADER_BUFFER_SPARE + 1;
		pReq->rawHeader.pvInData = pFC->AllocMem( pFC, pReq->rawHeader.cbInBuffer, 0);
		if( !pReq->rawHeader.pvInData ) {
			TRACE( TRACE_ERROR_SOFT, "AllocMem() failed!", 0 );
			return dwRet;
		}
		memcpy( pReq->rawHeader.pvInData, pRawData->pvInData, pReq->rawHeader.cbInData );
		// identify size of html data
		char* pin = FindContentLengthHeader( (char*)pRawData->pvInData, pRawData->cbInData );
		if( pin ) {
			// yup, convert it
			char* pout;
			pReq->dwOriginalSize = pReq->rawHtmlData.cbInBuffer = strtoul( pin, &pout, 10 );
			pReq->rawHtmlData.pvInData = pFC->AllocMem( pFC, pReq->rawHtmlData.cbInBuffer, 0 );
			// check memory here
			if( !pReq->rawHtmlData.pvInData ) {
				TRACE( TRACE_ERROR_SOFT, "AllocMem() failed!", 0 );
				return dwRet;
			}
			pReq->rawHtmlData.cbInData = 0;
		} else {
			// unknown original size
			pReq->dwOriginalSize = 0;
			// clear rawHtmlData buffer
			pReq->rawHtmlData.cbInBuffer = 0;
			pReq->rawHtmlData.pvInData = 0;
			pReq->rawHtmlData.cbInData = 0;
		}

		dwRet = SF_STATUS_REQ_READ_NEXT;
	} else {
		// Handle chunked reponse
		if(pReq->poDechunkifier) {
			CDechunkifier & oDechunkifier = *pReq->poDechunkifier;
			oDechunkifier.AddChunk(*pRawData);

			dwRet =  SF_STATUS_REQ_READ_NEXT;
		}
		// Non-chunked response
		else if( pReq->rawHtmlData.cbInBuffer ) {
			DWORD size = pRawData->cbInData;
			// calculate how much data is left in the buffer
			if ( size > pReq->rawHtmlData.cbInBuffer - pReq->rawHtmlData.cbInData )
				size = pReq->rawHtmlData.cbInBuffer - pReq->rawHtmlData.cbInData;
			memcpy( (char*)pReq->rawHtmlData.pvInData + pReq->rawHtmlData.cbInData, pRawData->pvInData, size );
			pReq->rawHtmlData.cbInData += size;
			
			if( pReq->rawHtmlData.cbInData >= pReq->rawHtmlData.cbInBuffer ) {
				// All the data is buffered, no more raw data send notifications required
				pFC->ServerSupportFunction( pFC, SF_REQ_DISABLE_NOTIFICATIONS, 0, SF_NOTIFY_SEND_RAW_DATA,0 );

				dwRet = SF_STATUS_REQ_NEXT_NOTIFICATION;
			} else {
				dwRet = SF_STATUS_REQ_READ_NEXT;
			}
		}
	}
	
	// Send everything in SF_NOTIFY_END_OF_REQUEST handler 
	pRawData->cbInData = 0;
	return dwRet;
}


/**
Compress and send the response
Clean up any allocated resources
*/
DWORD HandleEvent_EndOfRequest(HTTP_FILTER_CONTEXT* pFC)
{
	// Make sure no more raw data notifications are received after this event
	DisableFurtherNotificationsForThisRequest(pFC);

	PREQ_CONTEXT pReq = (PREQ_CONTEXT)pFC->pFilterContext;

	// Bail if pReq is NULL, nothing to do
	if( !pReq || !pReq->bDoEncoding) { return SF_STATUS_REQ_NEXT_NOTIFICATION; }

	PHTTP_FILTER_RAW_DATA pRawData = &pReq->rawHtmlData;

	// Get the de-chunked data if this was a chunked response
	if(pReq->poDechunkifier)
	{
		HTTP_FILTER_RAW_DATA oDechunkedData = pReq->poDechunkifier->GetRawDataStruct();
		HFRDCopy( pRawData, &oDechunkedData );
	}

	DoCompression( pReq->tEncoding, pFC, pRawData );
	ReplaceHeader( &(pReq->rawHeader), pRawData->cbInData );

	// Write header 
	pFC->WriteClient( pFC, pReq->rawHeader.pvInData, &pReq->rawHeader.cbInData, 0 );
	// Write data
	pFC->WriteClient( pFC, pRawData->pvInData, &pRawData->cbInData, 0 );

	// Clean up any memory allocated for this request
	delete pReq->poDechunkifier;
	pReq->poDechunkifier = NULL;

	// pFilterContext was allocated using ISAPI's AllocMem
	// which will be released when then connection closes
	pFC->pFilterContext = NULL;

	return SF_STATUS_REQ_NEXT_NOTIFICATION;
}

// 
// main filter proc
//
DWORD WINAPI HttpFilterProc( HTTP_FILTER_CONTEXT *pFC, 
							DWORD dwNotificationType, VOID *pvData)
{
	DWORD dwRet = SF_STATUS_REQ_NEXT_NOTIFICATION;
	switch( dwNotificationType )
	{
	case SF_NOTIFY_PREPROC_HEADERS:
		dwRet = HandleEvent_PreprocHeaders( pFC, (PHTTP_FILTER_PREPROC_HEADERS)pvData );
		break;
	case SF_NOTIFY_SEND_RESPONSE:
		dwRet = HandleEvent_SendResponse( pFC, (PHTTP_FILTER_PREPROC_HEADERS)pvData );
		break;
	case SF_NOTIFY_SEND_RAW_DATA:
		dwRet = HandleEvent_SendRawData( pFC, (PHTTP_FILTER_RAW_DATA)pvData );
		break;
	case SF_NOTIFY_END_OF_REQUEST:
		dwRet = HandleEvent_EndOfRequest(pFC);
		break;
	}

	return dwRet;
}

// Dll Entry point
BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
    switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH:
			break;
    }
    return TRUE;
}


