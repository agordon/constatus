// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "filter_motion_only.h"
#include "log.h"

filter_motion_only::filter_motion_only(selection_mask *const pixel_select_bitmap, const int noise_level) : pixel_select_bitmap(pixel_select_bitmap), noise_level(noise_level)
{
	prev1 = prev2 = NULL;
}

filter_motion_only::~filter_motion_only()
{
	delete pixel_select_bitmap;

	free(prev1);
	free(prev2);
}

void filter_motion_only::apply(const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out)
{
	if (!prev)
		return;

	size_t n = w * h * 3;

	if (prev1 == NULL) {
		prev1 = (uint8_t *)valloc(n);
		prev2 = (uint8_t *)valloc(n);
	}

	memcpy(prev1, in_out, n);

	const int nl3 = noise_level * 3;

	uint8_t *psb = pixel_select_bitmap ? pixel_select_bitmap -> get_mask(w, h) : NULL;

	if (psb) {
		for(int i=0; i<w*h; i++) {
			if (!psb[i])
				continue;

			int i3 = i * 3;

			int lc = in_out[i3 + 0] + in_out[i3 + 1] + in_out[i3 + 2];
			int lp = prev2[i3 + 0] + prev2[i3 + 1] + prev2[i3 + 2];

			if (abs(lc - lp) < nl3)
				in_out[i3 + 0] = in_out[i3 + 1] = in_out[i3 + 2] = 0;
		}
	}
	else {
		for(int i=0; i<w*h*3; i += 3) {
			int lc = in_out[i + 0] + in_out[i + 1] + in_out[i + 2];
			int lp = prev2[i + 0] + prev2[i + 1] + prev2[i + 2];

			if (abs(lc - lp) < nl3)
				in_out[i + 0] = in_out[i + 1] = in_out[i + 2] = 0;
		}
	}

	memcpy(prev2, prev1, n);
}
