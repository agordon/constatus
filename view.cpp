#include "view.h"
#include "http_server.h"
#include "utils.h"

view::view(configuration_t *const cfg, const std::string & id, const std::string & descr, const int width, const int height, const std::vector<std::string> & sources, const int gwidth, const int gheight) : interface(id, descr), cfg(cfg), width(width), height(height), sources(sources), grid_width(gwidth), grid_height(gheight)
{
}

view::~view()
{
}

std::string dim(const std::string & name, const int v)
{
	if (v != -1)
		return myformat(" %s=%d", name.c_str(), v);

	return "";
}

std::string dim_str(const int width, const int height)
{
	std::string s;

	if (width != -1)
		s += dim("width", width);

	if (height != -1)
		s += dim("height", height);

	return s;
}

std::string view::get_html() const
{
	std::string out = "<html><body>";

	int each_w = -1, each_h = -1;
	if (width != -1)
		each_w = width / grid_width;
	if (height != -1)
		each_h = height / grid_height;

	out += myformat("<table%s>", dim_str(width, height).c_str());

	size_t nr = 0;

	for(int y=0; y<grid_height; y++) {

		out += myformat("<tr%s>", dim_str(width, height).c_str());

		for(int x=0; x<grid_width; x++)
			out += myformat("<td><img src=\"%s\"%s></td>", http_server::mjpeg_stream_url(cfg, sources.at(nr++)).c_str(), dim_str(each_w, each_h).c_str());
	}

	out += "</table>";
	out += "</body></html>";

	return out;
}

void view::operator()()
{
}
