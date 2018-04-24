// (C) 2017-2018 by folkert van heusden, released under AGPL v3.0
#pragma once

#include <stdint.h>
#include <vector>

#include "cfg.h"

// NULL filter
class filter
{
protected:
	filter();

public:
	virtual ~filter();

	virtual bool uses_in_out() const { return true; }
	virtual void apply_io(instance_t *const i, const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out) = 0;
	virtual void apply(instance_t *const i, const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out) = 0;
};

void apply_filters(instance_t *const i, const std::vector<filter *> *const filters, const uint8_t *const prev, uint8_t *const work, const uint64_t ts, const int w, const int h);

void free_filters(const std::vector<filter *> *filters);
