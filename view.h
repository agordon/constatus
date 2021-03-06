// (C) 2017-2018 by folkert van heusden, released under AGPL v3.0
#pragma once
#include <string>
#include <vector>

#include "cfg.h"
#include "interface.h"
#include "source.h"

class view : public source
{
protected:
	configuration_t *const cfg;
	const int width, height;
	const std::vector<std::string> sources;

public:
	view(configuration_t *const cfg, const std::string & id, const std::string & descr, const int width, const int height, const std::vector<std::string> & sources);
	virtual ~view();

	virtual std::string get_html(const std::map<std::string, std::string> & pars) const = 0;

	virtual bool get_frame(const encoding_t pe, const int jpeg_quality, uint64_t *ts, int *width, int *height, uint8_t **frame, size_t *frame_len) = 0;

	virtual source *get_current_source();

	void operator()();
};

view *find_view(instance_t *const inst);
