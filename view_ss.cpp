// (C) 2017-2018 by folkert van heusden, released under AGPL v3.0
#include "view_ss.h"
#include "http_server.h"
#include "utils.h"
#include "log.h"

view_ss::view_ss(configuration_t *const cfg, const std::string & id, const std::string & descr, const int width, const int height, const std::vector<std::string> & sources, const double switch_interval, std::vector<target *> *const targets) : view(cfg, id, descr, width, height, sources), switch_interval(switch_interval), targets(targets)
{
	cur_source_index = 0;
        latest_switch = 0;
	cur_source = this;
}

view_ss::~view_ss()
{
	for(target *t : *targets)
		delete t;
	delete targets;
}

void view_ss::operator()()
{
	log(LL_INFO, "Starting %zu targets", targets -> size());

	for(size_t i=0; i<targets -> size(); i++)
		targets -> at(i) -> start(NULL, 0);

	log(LL_INFO, "view_ss thread terminating");
}

std::string view_ss::get_html(const std::map<std::string, std::string> & pars) const
{
	std::string dim_img;
	if (width != -1)
		dim_img += myformat(" width=%d", width);
	if (height != -1)
		dim_img += myformat(" height=%d", height);

	// FIXME url-escape
	return myformat("<html><head><style></style></head><body><img src=\"stream.mjpeg?int=%s&view-proxy\"%s></body></html>", id.c_str(), dim_img.c_str());
}

source *view_ss::get_current_source()
{
	return cur_source;
}

bool view_ss::get_frame(const encoding_t pe, const int jpeg_quality, uint64_t *ts, int *width, int *height, uint8_t **frame, size_t *frame_len)
{
	instance_t *inst = NULL;
	find_by_id(cfg, sources.at(cur_source_index), &inst, (interface **)&cur_source);

	uint64_t si = switch_interval * 1000000.0;
	if (switch_interval < 1.0)
		si = 1000000;

	if (latest_switch == 0)
		latest_switch = get_us();

	uint64_t now = get_us();
	if (now - latest_switch >= si) {
		// FIXME skip paused/stopped sources
		if (++cur_source_index >= sources.size())
			cur_source_index = 0;

		latest_switch = now;
	}

	if (!cur_source)
		return false;

	cur_source -> register_user();
	bool rc = cur_source -> get_frame(pe, jpeg_quality, ts, width, height, frame, frame_len);
	cur_source -> unregister_user();

	return rc;
}

void view_ss::stop()
{
	log(LL_INFO, "view_ss::stop");

	for(target *t : *targets) {
		t -> stop();
		delete t;
	}

	targets -> clear();

	interface::stop();
}
