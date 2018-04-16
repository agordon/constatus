#include <stdlib.h>

#include "resize.h"
#include "log.h"

resize::resize()
{
	log(LL_INFO, "Generic resizer instantiated");
}

resize::~resize()
{
}

void resize::do_resize(const int win, const int hin, const uint8_t *const in, const int wout, const int hout, uint8_t **out)
{
	*out = (uint8_t *)valloc(wout * hout * 3);

	const int max_offset = wout * hout * 3 - 3;

	const double maxw = std::max(win, wout);
	const double maxh = std::max(hin, hout);

	const double hins = hin / maxh;
	const double wins = win / maxw;
	const double houts = hout / maxh;
	const double wouts = wout / maxw;

	for(int y=0; y<maxh; y++) {
		const int in_scaled_y = y * hins;
		const int in_scaled_o = in_scaled_y * win * 3;
		const int out_scaled_y = y * houts;
		const int out_scaled_o = out_scaled_y * wout * 3;

		for(int x=0; x<maxw; x++) {
			int ino = in_scaled_o + int(x * wins) * 3;
			int outo = out_scaled_o + int(x * wouts) * 3;

			outo = std::min(max_offset, outo);

			(*out)[outo + 0] = in[ino + 0];
			(*out)[outo + 1] = in[ino + 1];
			(*out)[outo + 2] = in[ino + 2];
		}
	}
}
