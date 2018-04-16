// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <stdint.h>
#include <string>

#include "filter.h"
#include "source.h"

typedef enum
{
	none,
	upper_left,
	upper_center,
	upper_right,
	center_left,
	center_center,
	center_right,
	lower_left,
	lower_center,
	lower_right
} text_pos_t;

void find_text_dim(const char *const in, int *const n_lines, int *const n_cols);
std::string unescape(const std::string & in, const uint64_t ts, source *const s);

class filter_add_text : public filter
{
private:
	std::string what;
	text_pos_t tp;
	source *const s;

public:
	filter_add_text(const std::string & what, const text_pos_t tp, source *const s);
	~filter_add_text();

	bool uses_in_out() const { return false; }
	void apply(const uint64_t ts, const int w, const int h, const uint8_t *const prev, uint8_t *const in_out);
};
