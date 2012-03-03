//
//	Project: FlatCompression
//
//	ISAPI Filter for gzip/deflate compression
//
//	------------------------------------------
//	trace.h
//
//	Definition of trace commands
//	separates the filter from the tracing, allows to implement
//	different tracing algorithms
//	
//	Project created/started by Uli Margull / 1mal1 Software GmbH
//	Version	Date		Who			What
//	0.01	2002-06-19	U.Margull	Preparing Open Source Release:
//									- removing 1mal1 logging, adding trace

#ifndef TRACE_INCLUDE_
#define TRACE_INCLUDE_

#include "debugtools.h" 

#define TRACE_MESSAGE		0
#define TRACE_ERROR_SOFT	1
#define TRACE_ERROR_HARD	2

// comment the following line out then you get an build-in
// html logging
#define ENABLE_TRACE_HTML
#ifdef ENABLE_TRACE_HTML
#include "trace_html.h"
#define TRACE_INITIALIZE(logsize,logfile) 	trace_html_initialize((logsize), (logfile))
#define TRACE(type,text,param) trace_html( __FILE__, __LINE__, (type), (text), (param) )
#else
#define TRACE_INITIALIZE() 	
#define TRACE(type,text,param) _OADS("FlatCompression %d: %s (%d%)", (type), (text), (param) )
#endif

#endif