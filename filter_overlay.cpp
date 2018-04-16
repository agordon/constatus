// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <stddef.h>
#include <stdio.h>
#include <string>
#include <string.h>

#include "error.h"
#include "filter_overlay.h"
#include "picio.h"

filter_overlay::filter_overlay(const std::string & file, const int xIn, const int yIn) : x(xIn), y(yIn)
{
	FILE *fh = fopen(file.c_str(), "rb");
	if (!fh)
		error_exit(true, "%s failed to read", file.c_str());

	read_PNG_file_rgba(fh, &w, &h, &pixels);

	fclose(fh);
}

filter_overlay::~filter_overlay()
{
	free(pixels);
}

void filter_overlay::apply(const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out)
{
	int cw = std::min(this -> w, w);
	int ch = std::min(this -> h, h);

	if (cw <= 0 || ch <= 0)
		return;

	int ex = std::min(w - this -> x, cw);
	int ey = std::min(h - this -> y, ch);

	for(int y=0; y<ey; y++) {
		const int ty = y + this -> y;

		for(int x=0; x<ex; x++) {
			const int tx = x + this -> x;

			const int out_offset = ty * w * 3 + tx * 3;
			const int pic_offset = y * this -> w * 4 + x * 4;

			uint8_t alpha = pixels[pic_offset + 3], ialpha = 255 - alpha;
			in_out[out_offset + 0] = (int(pixels[pic_offset + 0]) * alpha + (in_out[out_offset + 0] * ialpha)) / 256;
			in_out[out_offset + 1] = (int(pixels[pic_offset + 1]) * alpha + (in_out[out_offset + 1] * ialpha)) / 256;
			in_out[out_offset + 2] = (int(pixels[pic_offset + 2]) * alpha + (in_out[out_offset + 2] * ialpha)) / 256;
		}
	}
}
