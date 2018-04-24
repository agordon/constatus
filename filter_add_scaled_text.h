// (C) 2017-2018 by folkert van heusden, released under AGPL v3.0
#include <stdint.h>
#include <string>

#include "filter.h"
#include "cfg.h"

class filter_add_scaled_text : public filter
{
private:
	std::string what, font_name;
	const int x, y, font_size, r, g, b;

public:
	filter_add_scaled_text(const std::string & what, const std::string & font_name, const int x, const int y, const int font_size, const int r, const int g, const int b);
	~filter_add_scaled_text();

	bool uses_in_out() const { return true; }
	void apply_io(instance_t *const i, const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out);
};
