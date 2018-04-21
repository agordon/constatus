// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <atomic>
#include <string>
#include <thread>

#include "source.h"

class source_stream : public source
{
private:
	const std::string url;
	const bool tcp;

public:
	source_stream(const std::string & id, const std::string & descr, const std::string & url, const bool tcp, const double max_fps, resize *const r, const int resize_w, const int resize_h, const int loglevel, const double timeout);
	~source_stream();

	void operator()();
};
