// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <string>

#include "filter.h"

typedef void * (*init_filter_t)(const char *const par);
typedef void (*apply_filter_t)(void *arg, const uint64_t ts, const int w, const int h, const uint8_t *const prev_frame, const uint8_t *const current_frame, uint8_t *const result);
typedef void (*uninit_filter_t)(void *arg);

class filter_plugin : public filter
{
private:
	void *library;
	init_filter_t init_filter;
	apply_filter_t apply_filter;
	uninit_filter_t uninit_filter;
	void *arg;

public:
	filter_plugin(const std::string & filter_filename, const std::string & parameter);
	~filter_plugin();

	bool uses_in_out() const { return true; }
	void apply_io(const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out);
};
