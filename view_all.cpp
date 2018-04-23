// (C) 2017-2018 by folkert van heusden, released under AGPL v3.0
#include "view_all.h"
#include "http_server.h"
#include "utils.h"

view_all::view_all(configuration_t *const cfg, const std::string & id, const std::string & descr, const std::vector<std::string> & sources) : view(cfg, id, descr, -1, -1, sources)
{
}

view_all::~view_all()
{
}

std::string view_all::get_html() const
{
	std::string reply = "<head>"
				"<link href=\"stylesheet.css\" rel=\"stylesheet\" media=\"screen\">"
				"<link rel=\"shortcut icon\" href=\"favicon.ico\">"
				"<title>" NAME " " VERSION "</title>"
				"</head>"
				"<div class=\"vidmain\">";

	for(instance_t * inst : cfg -> instances) {
		source *const s = find_source(inst);

		int w = s -> get_width();
		int use_h = s -> get_height() * (320.0 / w);

		reply += myformat("<div class=\"vid\"><p><a href=\"index.html?inst=%s\">%s</p><img src=\"stream.mjpeg?inst=%s\" width=320 height=%d></a></div>", url_escape(inst -> name).c_str(), inst -> name.c_str(), url_escape(inst -> name).c_str(), use_h);
	}

	reply += "</div>";

	return reply;
}

void view_all::operator()()
{
}
