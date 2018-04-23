// (C) 2017-2018 by folkert van heusden, released under AGPL v3.0
#include <vector>

#include "source.h"
#include "filter.h"
#include "cfg.h"
#include "view.h"

class http_server : public interface
{
private:
	instance_t *const inst;
	const double fps;
	const int quality, time_limit;
	const std::vector<filter *> *const f;
	resize *const r;
	const int resize_w, resize_h;
	const bool motion_compatible, allow_admin, archive_acces;
	configuration_t *const cfg;
	const std::string snapshot_dir;
	const bool is_rest;
	std::vector<view *> *const views;

	int fd;

public:
	http_server(configuration_t *const cfg, const std::string & id, const std::string & descr, instance_t *const inst, const std::string & http_adapter, const int http_port, const double fps, const int quality, const int time_limit, const std::vector<filter *> *const f, resize *const r, const int resize_w, const int resize_h, const bool motion_compatible, const bool allow_admin, const bool archive_access, const std::string & snapshot_dir, const bool is_rest, std::vector<view *> *const views);
	virtual ~http_server();

	static void mjpeg_stream_url(configuration_t *const cfg, const std::string & id, std::string *const img_url, std::string *const page_url);

	void operator()();
};
