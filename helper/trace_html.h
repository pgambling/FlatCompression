//
//	Project: FlatCompression
//
//	ISAPI Filter for gzip/deflate compression
//
//	------------------------------------------
//	trace_html.h
//
//	Header file for simple html logging 
//	
//	Project created/started by Uli Margull / 1mal1 Software GmbH
//	Version	Date		Who			What
//	0.10	2002-09-17	U.Margull	First implementation 

#ifndef TRACE_HTML_INCLUDE_
#define TRACE_HTML_INCLUDE_

#include "trace.h"
void trace_html_initialize(int logsize, char* logname);
void trace_html( char*file, int line, int type, char* text, int param );
void trace_html_format( char* buf, int bufsize, int n_lines, char* line_fmt );
char* trace_html_header(int type);

#endif