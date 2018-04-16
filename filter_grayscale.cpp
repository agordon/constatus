// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <stddef.h>
#include <string.h>

#include "filter_grayscale.h"

filter_grayscale::filter_grayscale()
{
}

filter_grayscale::~filter_grayscale()
{
}

void filter_grayscale::apply_io(const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *in, uint8_t *out)
{
	for(int i=0; i<w*h; i++) {
		const uint8_t g = (*in++ + *in++ + *in++) / 3;

		*out++ = *out++ = *out++ = g;
	}
}
