// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include "filter.h"

class filter_mirror_v : public filter
{
public:
	filter_mirror_v();
	~filter_mirror_v();

	bool uses_in_out() const { return true; }
	void apply_io(const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out);
};
