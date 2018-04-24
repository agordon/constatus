// (C) 2017-2018 by folkert van heusden, released under AGPL v3.0
#include <stddef.h>
#include <string.h>

#include "filter_mirror_v.h"

filter_mirror_v::filter_mirror_v()
{
}

filter_mirror_v::~filter_mirror_v()
{
}

void filter_mirror_v::apply_io(instance_t *const i, interface *const specific_int, const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out)
{
	const size_t bytes = w * 3;

	for(int y=0; y<h; y++)
		memcpy(&out[(h - y - 1) * bytes], &in[y * bytes], bytes);
}
