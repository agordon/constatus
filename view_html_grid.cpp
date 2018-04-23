// (C) 2017-2018 by folkert van heusden, released under AGPL v3.0
#include "view_html_grid.h"
#include "http_server.h"
#include "utils.h"

view_html_grid::view_html_grid(configuration_t *const cfg, const std::string & id, const std::string & descr, const int width, const int height, const std::vector<std::string> & sources, const int gwidth, const int gheight) : view(cfg, id, descr, width, height, sources), grid_width(gwidth), grid_height(gheight)
{
}

view_html_grid::~view_html_grid()
{
}

std::string view_html_grid::get_html() const
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

	std::string dim_css;
	if (width != -1)
		dim_css += myformat("width:%dpx;", width);
	if (height != -1)
		dim_css += myformat("height:%dpx;", height);

	std::string out = myformat("<html><head><style>.grid-container {\n%s;display: grid;\ngrid-template-columns: %s;\ngrid-column-gap:5px;\ngrid-row-gap:5px;grid-template-rows:%s;\n}\n.grid-item { }\n</style></head><body>", dim_css.c_str(), col_size.c_str(), row_size.c_str());

	std::string dim_img;
	int each_w = -1, each_h = -1;
	if (width != -1) {
		each_w = width / grid_width;
		dim_img += myformat(" width=%d", each_w);
	}
	if (height != -1) {
		each_h = height / grid_height;
		dim_img += myformat(" height=%d", each_h);
	}

	out += myformat("<div class=\"grid-container\">");

	size_t nr = 0;

	for(int y=0; y<grid_height; y++) {
		for(int x=0; x<grid_width; x++) {
			std::string img_url, page_url;
			http_server::mjpeg_stream_url(cfg, sources.at(nr++), &img_url, &page_url);

			out += myformat("<div class=\"grid-item\"><a href=\"%s\"><img src=\"%s\"%s></a></div>", page_url.c_str(), img_url.c_str(), dim_img.c_str());

			if (nr == sources.size())
				goto done;
		}
	}

done:
	out += "</div>";
	out += "</body></html>";

	return out;
}

void view_html_grid::operator()()
{
}
