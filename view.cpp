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
	std::string col_size, row_size;
	for(int i=0; i<grid_width; i++) {
		if (i)
			col_size += " max-content";
		else
			col_size = "max-content";
	}
	for(int i=0; i<grid_height; i++) {
		if (i)
			row_size += " max-content";
		else
			row_size = "max-content";
	}

	std::string out = myformat("<html><head><style>.grid-container {\ndisplay: grid;\ngrid-template-columns: %s;\ngrid-column-gap:5px;\ngrid-row-gap:5px;grid-template-rows:%s;\n}\n.grid-item { }\n</style></head><body>", col_size.c_str(), row_size.c_str());

	int each_w = -1, each_h = -1;
	if (width != -1)
		each_w = width / grid_width;
	if (height != -1)
		each_h = height / grid_height;

	out += myformat("<div class=\"grid-container\">");

	size_t nr = 0;

	for(int y=0; y<grid_height; y++) {
		for(int x=0; x<grid_width; x++) {
			out += myformat("<div class=\"grid-item\"><img src=\"%s\"%s></div>", http_server::mjpeg_stream_url(cfg, sources.at(nr++)).c_str(), dim_str(each_w, each_h).c_str());

			if (nr == sources.size())
				goto done;
		}
	}

done:
	out += "</div>";
	out += "</body></html>";

	return out;
}

void view::operator()()
{
}
