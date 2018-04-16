// (C) 2017 by folkert van heusden, released under AGPL v3.0
#pragma once

#include <stdint.h>
#include <vector>

// NULL filter
class filter
{
public:
	filter();
	virtual ~filter();

	virtual bool uses_in_out() const { return true; }
	virtual void apply_io(const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out);
	virtual void apply(const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out);
};

void apply_filters(const std::vector<filter *> *const filters, const uint8_t *const prev, uint8_t *const work, const uint64_t ts, const int w, const int h);

void free_filters(const std::vector<filter *> *filters);
