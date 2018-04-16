#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "resize_crop.h"
#include "log.h"

resize_crop::resize_crop()
{
	log(LL_INFO, "Generic resize_crop instantiated");
}

resize_crop::~resize_crop()
{
}

void resize_crop::do_resize_crop(const int win, const int hin, const uint8_t *const in, const int wout, const int hout, uint8_t **out)
{
	size_t out_size = wout * hout * 3;

	*out = (uint8_t *)valloc(out_size);

	memset(*out, 0x00, out_size);

	for(int y=0; y<std::min(hin, hout); y++)
		memcpy(&(*out)[y * wout * 3], &in[y * win * 3], std::min(win, wout) * 3);
}
