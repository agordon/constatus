// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include "filter.h"

class filter_boost_contrast : public filter
{
public:
	filter_boost_contrast();
	~filter_boost_contrast();

	bool uses_in_out() const { return false; }
	void apply(const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out);
};
