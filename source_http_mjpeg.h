// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <condition_variable>
#include <pthread.h>
#include <string>
#include <thread>

#include "source.h"

class source_http_mjpeg : public source
{
private:
	const std::string url;
	const bool ignore_cert;

public:
	source_http_mjpeg(const std::string & id, const std::string & url, const bool ignore_cert, const double max_fps, resize *const r, const int resize_w, const int resize_h, const int loglevel, const double timeout);
	virtual ~source_http_mjpeg();

	void operator()();
};
