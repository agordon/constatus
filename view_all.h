// (C) 2017-2018 by folkert van heusden, released under AGPL v3.0
#pragma once
#include <map>
#include <string>
#include <vector>

#include "cfg.h"
#include "view.h"
#include "source.h"

class view_all : public view
{
private:
	bool get_frame(const encoding_t pe, const int jpeg_quality, uint64_t *ts, int *width, int *height, uint8_t **frame, size_t *frame_len) { return false; }

public:
	view_all(configuration_t *const cfg, const std::string & id, const std::string & descr, const std::vector<std::string> & sources);
	virtual ~view_all();

	std::string get_html(const std::map<std::string, std::string> & pars) const;

	void operator()();
};
