// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <stdlib.h>
#include <string.h>

#include "filter.h"
#include "log.h"

void apply_filters(const std::vector<filter *> *const filters, const uint8_t *const prev, uint8_t *const work, const uint64_t ts, const int w, const int h)
{
	const size_t bytes = w * h * 3;
	uint8_t *const temp = (uint8_t *)valloc(bytes);

	bool flag = false;
	for(filter *f : *filters) {
		if (f -> uses_in_out()) {
			if (flag == false)
				f -> apply_io(ts, w, h, prev, work, temp);
			else
				f -> apply_io(ts, w, h, prev, temp, work);

			flag = !flag;
		}
		else {
			if (flag == false)
				f -> apply(ts, w, h, prev, work);
			else
				f -> apply(ts, w, h, prev, temp);
		}
	}

	if (flag == true)
		memcpy(work, temp, bytes);

	free(temp);
}

void free_filters(const std::vector<filter *> *filters)
{
	for(filter *f : *filters)
		delete f;
}

filter::filter()
{
}

filter::~filter()
{
}

void filter::apply_io(const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out)
{
	log(LL_FATAL, "filter::apply_io should not be called");
}

void filter::apply(const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out)
{
	log(LL_FATAL, "filter::apply should not be called");
}
