//
//	Project: FlatCompression
//
//	ISAPI Filter for gzip/deflate compression
//
//	------------------------------------------
//	trace_html.cpp
//
//	Implementation file for simple html logging 
//	
//	Project created/started by Uli Margull / 1mal1 Software GmbH
//	Version	Date		Who			What
//	0.10	2002-09-17	U.Margull	First implementation 

#include "stdafx.h"
#include <stdio.h>
#include "trace_html.h"
#include "time.h"

#define MAX_TEXT_LEN	500

// define data structure for logging
typedef struct 
{
	time_t date;
	char file[MAX_TEXT_LEN+1];
	int line;
	int type;
	char text[MAX_TEXT_LEN+1];
	int param;
} LOG_TYPE;

// define ring buffer settings
static int nnext = 0;
static int nlast = 0;

static int max_log = 0;
static LOG_TYPE* myLog = 0;

void trace_html_initialize(int logsize, char* logfile)
{
	if( (logsize>0) && (max_log==0) ) {
		nnext = nlast = 0;
		if( logsize>8192 ) 
			logsize=8192;
		max_log = logsize;
		myLog = new LOG_TYPE[max_log];
		// dont care about freeing this memory ever
		if( myLog==0 )
			max_log = 0;
	}

}

static void inc_counter_(int* nn)
{
	if( *nn >= max_log-1 ) *nn = 0;
	else ++(*nn);
}

static void inc_counter ( int* nn, int* nl )
{
	// increase next counter
	inc_counter_(nn);
	if( *nn==*nl ) 
		// increase last counter
		inc_counter_(nl);
}

void trace_html( char*file, int line, int type, char* text, int param )
{
	// are we initialized?
	if( myLog==0 )
		return;

	// add to buffer 
	// synchronizing? 
	// this is not really synchronized
	// so some logs can be broken,
	// but this should suffice for now

	// increase counter, be sure to not
	// produce overrun in multithreaded
	// environment
	int n = nnext;
	inc_counter( &nnext, &nlast ); 

	// save data
	LOG_TYPE * ptr = myLog + n;
	ptr->date = time(0);
	strncpy( ptr->file, file, MAX_TEXT_LEN-1 );
	ptr->line = line;
	ptr->type = type;
	strncpy( ptr->text, text, MAX_TEXT_LEN-1 );
	ptr->param = param;

	// done

}

static char header_fmt_debug[] = "<tr><th>Type</th><th>Message</th><th>Param</th><th>Time</th><th>File</th><th>Line</th></tr>";
static char row_fmt_debug[] = "<tr>%s<td>%s</td><td>%d</td><td>%s</td><td>%s</td><td>%d</td></tr>";

static char* message_fmt[] = {
	"<td bgcolor=lime>Info</td>",
	"<td bgcolor=yellow>Warning</td>",
	"<td bgcolor=red>ERROR</td>"
};

char* trace_html_header(int type)
{
	return header_fmt_debug;
}

void trace_html_format( char* buf, int bufsize, int n_lines, char* line_fmt )
{
	// are we initialized?
	if( myLog==0 )
		return;

	// line buffer
	char lbuf[MAX_TEXT_LEN+1];
	LOG_TYPE* ptr;
	int nsize;
	int bufsize_left = bufsize;
	char* bptr = buf;
	
	// check parameters
	if( (n_lines<=0) || (n_lines>=max_log) )
		n_lines = max_log-1;

	// format the last n_lines
	int nn = nnext;
	int nl = nlast;

	// now loop around
	while( (n_lines>0) && (nn!=nl) ) {

		// decrease counter
		--nn;
		if( nn<0 ) 
			nn = max_log-1;

		ptr = myLog+nn;

		wsprintf( lbuf, row_fmt_debug, 
			((ptr->type>=0)&&(ptr->type<3))?message_fmt[ptr->type]:"<td>??</td>",
			ptr->text,
			ptr->param,
			ctime( & ptr->date ), 
			ptr->file,
			ptr->line
			);

		nsize = strlen(lbuf);
		if( nsize < bufsize_left ) {
			bufsize_left -= nsize;
			// copy with trailing zero
			memcpy( bptr, lbuf, nsize + 1 );
			bptr += nsize;
		} else {
			break;
		}


	} 

}