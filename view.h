#pragma once
#include <string>
#include <vector>

#include "cfg.h"
#include "interface.h"
#include "source.h"

class view : public interface
{
private:
	configuration_t *const cfg;
	const int width, height;
	const std::vector<std::string> sources;
	const int grid_width, grid_height;

public:
	view(configuration_t *const cfg, const std::string & id, const std::string & descr, const int width, const int height, const std::vector<std::string> & sources, const int gwidth, const int gheight);
	virtual ~view();

	std::string get_html() const;

	void operator()();
};
