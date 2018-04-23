// (C) 2017-2018 by folkert van heusden, released under AGPL v3.0
#include "view.h"
#include "http_server.h"
#include "utils.h"

view::view(configuration_t *const cfg, const std::string & id, const std::string & descr, const int width, const int height, const std::vector<std::string> & sources) : interface(id, descr), cfg(cfg), width(width), height(height), sources(sources)
{
}

view::~view()
{
}

void view::operator()()
{
}
