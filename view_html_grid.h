// (C) 2017-2018 by folkert van heusden, released under AGPL v3.0
#pragma once
#include <string>
#include <vector>

#include "cfg.h"
#include "view.h"
#include "source.h"

class view_html_grid : public view
{
private:
	const int grid_width, grid_height;

	bool get_frame(const encoding_t pe, const int jpeg_quality, uint64_t *ts, int *width, int *height, uint8_t **frame, size_t *frame_len) { return false; }

public:
	view_html_grid(configuration_t *const cfg, const std::string & id, const std::string & descr, const int width, const int height, const std::vector<std::string> & sources, const int gwidth, const int gheight);
	virtual ~view_html_grid();

	std::string get_html() const;

	void operator()();
};
