// (C) 2017-2018 by folkert van heusden, released under AGPL v3.0
#pragma once

#include <stdint.h>

#include "filter.h"
#include "meta.h"
#include "selection_mask.h"

typedef enum { m_red, m_red_invert, m_invert } sm_mode_t;

class filter_marker_simple : public filter
{
private:
	const sm_mode_t mode;
	selection_mask *const pixel_select_bitmap;
	meta *const m;
	const int noise_level;
	const double percentage_pixels_changed;

	void updatePixel(uint8_t *const out, const int x, const int y, const int w);

public:
	filter_marker_simple(const sm_mode_t modeIn, selection_mask *const sb, meta *const m, const int noise_level, const double percentage_pixels_changed);
	virtual ~filter_marker_simple();

	bool uses_in_out() const { return false; }
	void apply(instance_t *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out);
};
