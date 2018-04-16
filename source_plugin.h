// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <atomic>
#include <string>
#include <thread>

#include "source.h"

typedef void *(* sp_init_plugin_t)(source *const s, const char *const argument);
typedef void (* sp_uninit_plugin_t)(void *arg);

class source_plugin : public source
{
private:
	sp_init_plugin_t init_plugin;
	sp_uninit_plugin_t uninit_plugin;
	void *arg, *library;

public:
	source_plugin(const std::string & id, const std::string & plugin_filename, const std::string & plugin_arg, const double max_fps, resize *const r, const int resize_w, const int resize_h, const int loglevel, const double timeout);
	~source_plugin();

	void operator()();
};
