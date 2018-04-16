// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <stddef.h>
#include <string.h>

#include "filter_mirror_h.h"

filter_mirror_h::filter_mirror_h()
{
}

filter_mirror_h::~filter_mirror_h()
{
}

void filter_mirror_h::apply_io(const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out)
{
	for(int y=0; y<h; y++) {
		const int yoff = y * w * 3;

		for(int x=0; x<w; x++) {
			int oin = yoff + x * 3;
			int oout = yoff + (w - x - 1) * 3;

			out[oout + 0] = in[oin + 0];
			out[oout + 1] = in[oin + 1];
			out[oout + 2] = in[oin + 2];
		}
	}
}
