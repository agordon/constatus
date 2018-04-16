#include "cairo.h"
#include "log.h"
#include "resize_cairo.h"

resize_cairo::resize_cairo()
{
	log(LL_INFO, "Cairo resizer instantiated");
}

resize_cairo::~resize_cairo()
{
}

void resize_cairo::do_resize(const int win, const int hin, const uint8_t *const in, const int wout, const int hout, uint8_t **out)
{
	uint32_t *temp = NULL;
	cairo_surface_t *work_in = rgb_to_cairo(in, win, hin, &temp);

	cairo_surface_t *result = cairo_surface_create_similar(work_in, cairo_surface_get_content(work_in), wout, hout);
	cairo_t *cr = cairo_create(result);
	cairo_scale(cr, double(win)/wout, double(hin)/hout);
	cairo_set_source_surface(cr, work_in, 0, 0);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cr);
	cairo_destroy(cr);

	*out = (uint8_t *)valloc(wout * hout * 3);
	cairo_to_rgb(result, wout, hout, *out);
	free(temp);
}
