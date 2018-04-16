// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include "filter.h"
#include "selection_mask.h"

class filter_apply_mask : public filter
{
private:
	selection_mask *const psb;

public:
	filter_apply_mask(selection_mask *const pixel_select_bitmap);
	~filter_apply_mask();

	bool uses_in_out() const { return false; }
	void apply(const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out);
};
