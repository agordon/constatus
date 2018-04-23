// (C) 2017-2018 by folkert van heusden, released under AGPL v3.0
#include "resize.h"

class resize_cairo : public resize
{
public:
	resize_cairo();
	~resize_cairo();

	void do_resize(const int win, const int hin, const uint8_t *const in, const int wout, const int hout, uint8_t **out);
};
