// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <vector>

#include "source.h"
#include "filter.h"
#include "cfg.h"

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

	int fd;

public:
	http_server(configuration_t *const cfg, const std::string & id, const std::string & descr, instance_t *const inst, const std::string & http_adapter, const int http_port, const double fps, const int quality, const int time_limit, const std::vector<filter *> *const f, resize *const r, const int resize_w, const int resize_h, const bool motion_compatible, const bool allow_admin, const bool archive_access, const std::string & snapshot_dir, const bool is_rest);
	virtual ~http_server();

	static std::string mjpeg_stream_url(configuration_t *const cfg, const std::string & id);

	void operator()();
};
