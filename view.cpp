// (C) 2017-2018 by folkert van heusden, released under AGPL v3.0
#include "view.h"
#include "http_server.h"
#include "utils.h"

view *find_view(instance_t *const inst)
{
	if (!inst)
		return NULL;

	for(interface *i : inst -> interfaces) {
		if (i -> get_class_type() == CT_VIEW)
			return (view *)i;
	}

	return NULL;
}

view::view(configuration_t *const cfg, const std::string & id, const std::string & descr, const int width, const int height, const std::vector<std::string> & sources) : source(id, descr), cfg(cfg), width(width), height(height), sources(sources)
{
	ct = CT_VIEW;
}

view::~view()
{
}

void view::operator()()
{
}

source *view::get_current_source()
{
	return this;
}
