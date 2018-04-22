#include "view.h"
#include "http_server.h"
#include "utils.h"

view::view(configuration_t *const cfg, const std::string & id, const std::string & descr, const int width, const int height, const std::vector<std::string> & sources, const int gwidth, const int gheight) : interface(id, descr), cfg(cfg), width(width), height(height), sources(sources), grid_width(gwidth), grid_height(gheight)
{
}

view::~view()
{
}

std::string view::get_html() const
{
	std::string out = "<html><body>";
	out += myformat("<table width=%d height=%d>", width, height);

	size_t nr = 0;

	for(int y=0; y<grid_height; y++) {
		out += "<tr>";

		for(int x=0; x<grid_width; x++)
			out += myformat("<td><img src=\"%s\"></td>", http_server::mjpeg_stream_url(cfg, sources.at(nr++)).c_str());
	}

	out += "</table>";
	out += "</body></html>";

	return out;
}

void view::operator()()
{
}
