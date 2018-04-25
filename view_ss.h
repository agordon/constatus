// (C) 2017-2018 by folkert van heusden, released under AGPL v3.0
#pragma once
#include <string>
#include <vector>

#include "cfg.h"
#include "view.h"
#include "source.h"
#include "target.h"

class view_ss : public view
{
private:
	const double switch_interval;
	std::vector<target *> *const targets;

	size_t cur_source_index;
	uint64_t latest_switch;
	source *cur_source;

public:
	view_ss(configuration_t *const cfg, const std::string & id, const std::string & descr, const int width, const int height, const std::vector<std::string> & sources, const double switch_interval, std::vector<target *> *const targets);
	virtual ~view_ss();

	std::string get_html(const std::map<std::string, std::string> & pars) const;
	bool get_frame(const encoding_t pe, const int jpeg_quality, uint64_t *ts, int *width, int *height, uint8_t **frame, size_t *frame_len);
	virtual source *get_current_source();

	void stop();
	void operator()();
};
