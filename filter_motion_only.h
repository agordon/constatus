// (C) 2017-2018 by folkert van heusden, released under AGPL v3.0
#pragma once

#include <stdint.h>

#include "filter.h"
#include "selection_mask.h"

class filter_motion_only : public filter
{
private:
	selection_mask *const pixel_select_bitmap;
	uint8_t *prev1, *prev2;
	const int noise_level;

public:
	filter_motion_only(selection_mask *const pixel_select_bitmap, const int noise_level);
	virtual ~filter_motion_only();

	bool uses_in_out() const { return false; }
	void apply(const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out);
};
