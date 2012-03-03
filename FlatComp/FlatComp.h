//
//	Project: FlatCompression
//
//	ISAPI Filter for gzip/deflate compression
//
//	------------------------------------------
//
//	FlatComp.h
//
//	Main ISAPI filter file
//	
//	Project created/started by Uli Margull / 1mal1 Software GmbH
//	Version	Date		Who			What
//	1.03	2002-06-19	U.Margull	Preparing Open Source Release:
//									- removing 1mal1 logging, adding trace

BOOL WINAPI GetFilterVersion(HTTP_FILTER_VERSION *pVersion);

DWORD WINAPI HttpFilterProc( HTTP_FILTER_CONTEXT *pFC, 
							DWORD dwNotificationType, VOID *pvData);
