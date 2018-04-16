// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <atomic>
#include <string>
#include <thread>

#include "source.h"

class source_rtsp : public source
{
private:
	const std::string url;
	const bool tcp;

public:
	source_rtsp(const std::string & id, const std::string & url, const bool tcp, const double max_fps, resize *const r, const int resize_w, const int resize_h, const int loglevel, const double timeout);
	~source_rtsp();

	void operator()();
};
