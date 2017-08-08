// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "filter_boost_contrast.h"

filter_boost_contrast::filter_boost_contrast()
{
}

filter_boost_contrast::~filter_boost_contrast()
{
}

void filter_boost_contrast::apply(const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out)
{
	uint8_t lowest_br = 255, highest_br = 0;

	for(int i=0; i<w*h*3; i+=3) {
		int g = (in[i + 0] + in[i + 1] + in[i + 2]) / 3;

		if (g < lowest_br)
			lowest_br = g;
		else if (g > highest_br)
			highest_br = g;
	}

	double mul = 255.0 / (highest_br - lowest_br);

	if ((highest_br < 255 || lowest_br > 0) && !isnan(mul) && !isinf(mul)){
		printf("%f\n", mul);

		for(int i=0; i<w*h*3; i+=3) {
			out[i + 0] = (in[i + 0] - lowest_br) * mul;
			out[i + 1] = (in[i + 1] - lowest_br) * mul;
			out[i + 2] = (in[i + 2] - lowest_br) * mul;
		}
	}
	else {
		memcpy(out, in, w * h * 3);
	}
}
