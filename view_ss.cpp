// (C) 2017-2018 by folkert van heusden, released under AGPL v3.0
#include "view_ss.h"
#include "http_server.h"
#include "utils.h"

view_ss::view_ss(configuration_t *const cfg, const std::string & id, const std::string & descr, const int width, const int height, const std::vector<std::string> & sources, const double switch_interval) : view(cfg, id, descr, width, height, sources), switch_interval(switch_interval)
{
	cur_source_index = 0;
        latest_switch = get_us();
	cur_source = this;
}

view_ss::~view_ss()
{
}

void view_ss::operator()()
{
}

std::string view_ss::get_html() const
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

	uint64_t now = get_us();
	if (now - latest_switch >= si) {
		// FIXME skip paused/stopped sources
		if (++cur_source_index >= sources.size())
			cur_source_index = 0;

		latest_switch = now;
	}

	cur_source -> register_user();
	bool rc = cur_source -> get_frame(pe, jpeg_quality, ts, width, height, frame, frame_len);
	cur_source -> unregister_user();

	return rc;
}
