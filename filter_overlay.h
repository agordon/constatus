// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <string>

#include "filter.h"

class filter_overlay : public filter
{
	int x, y, w, h;
	uint8_t *pixels;

public:
	filter_overlay(const std::string & file, const int x, const int y);
	~filter_overlay();

	bool uses_in_out() const { return false; }
	void apply(const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out);
};
