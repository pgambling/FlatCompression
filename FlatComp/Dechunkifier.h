/**
Takes chunked HTTP response data and returns a normalized HTTP_FILTER_RAW_DATA struct
*/

#pragma once 

#include <string>
#include <iostream>
#include <sstream>

class CDechunkifier
{
	std::stringstream m_oChunkedBuffer;
	HTTP_FILTER_RAW_DATA m_oDechunkedData;
	HTTP_FILTER_CONTEXT* m_pFC;

	void DechunkTheBuffer();
	unsigned CDechunkifier::GetChunkSize(std::string sLine);

public:
	CDechunkifier(HTTP_FILTER_CONTEXT* pFC);

	void AddChunk(const HTTP_FILTER_RAW_DATA &);
	HTTP_FILTER_RAW_DATA GetRawDataStruct();
};