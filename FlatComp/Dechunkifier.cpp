#include "stdafx.h"
#include <regex>
#include "Dechunkifier.h"

CDechunkifier::CDechunkifier(HTTP_FILTER_CONTEXT* pFC)
	:	m_oChunkedBuffer(std::stringstream::in | std::stringstream::out)
{
	m_pFC = pFC;

	m_oDechunkedData.cbInBuffer = m_oDechunkedData.cbInData = 0;
	m_oDechunkedData.pvInData = 0;
	m_oDechunkedData.dwReserved = 0; // This is always 0
}

void CDechunkifier::AddChunk(const HTTP_FILTER_RAW_DATA & oRawData)
{
	m_oChunkedBuffer.write(static_cast<const char *>(oRawData.pvInData), oRawData.cbInData);
}

HTTP_FILTER_RAW_DATA CDechunkifier::GetRawDataStruct()
{
	DechunkTheBuffer();

	return m_oDechunkedData;
}

void CDechunkifier::DechunkTheBuffer()
{
	m_oChunkedBuffer.seekg(0, std::ios::end);
	m_oDechunkedData.cbInBuffer = static_cast<unsigned long>(m_oChunkedBuffer.tellg());
	m_oChunkedBuffer.seekg(0, std::ios::beg);

	// Use the ISAPI memory allocator to create the dechunked data buffer
	// so it will survive the lifetime of this class and clean it up when no longer needed.
	m_oDechunkedData.pvInData = m_pFC->AllocMem( m_pFC, m_oDechunkedData.cbInBuffer, 0 );

	// Bail if the memory allocation failed
	if( m_oDechunkedData.pvInData == 0 ) { return; }

	char * pcDechunkBufferWriteLocation = static_cast<char *>(m_oDechunkedData.pvInData);
	while(m_oChunkedBuffer.good())
	{
		std::string sLine;
		getline(m_oChunkedBuffer, sLine);

		// Get size of next chunk
		unsigned uSize = GetChunkSize(sLine);

		// Read the chunk data into the buffer
		m_oChunkedBuffer.read(static_cast<char *>(pcDechunkBufferWriteLocation), uSize);
		pcDechunkBufferWriteLocation += uSize;
		m_oChunkedBuffer.ignore(uSize); // extract the read bytes from input stream
		
		// Update the raw data size
		m_oDechunkedData.cbInData += uSize;
	}
}

unsigned CDechunkifier::GetChunkSize(std::string sLine)
{
	std::regex oMatchChunkSize("^([[:xdigit:]]+)(;.*)?\r$");
	std::smatch oMatches;
    
    std::regex_search(sLine, oMatches, oMatchChunkSize);
    return strtol(oMatches.str(1).c_str(), NULL, 16);
}