// (C) 2017-2018 by folkert van heusden, released under AGPL v3.0
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <vector>

#include <cairo/cairo.h>

#include "picio.h"
#include "utils.h"

cairo_surface_t * rgb_to_cairo(const uint8_t *const in, const int w, const int h, uint32_t **temp)
{
	size_t n = w * h;
	*temp = (uint32_t *)valloc(n * 4);

	const uint8_t *win = in;
	uint32_t *wout = *temp;

	for(size_t i=0; i<n; i++) {
		uint8_t r = *win++;
		uint8_t g = *win++;
		uint8_t b = *win++;
		*wout++ = (255 << 24) | (r << 16) | (g << 8) | b;
	}

	return cairo_image_surface_create_for_data((unsigned char *)*temp, CAIRO_FORMAT_RGB24, w, h, 4 * w);
}

void cairo_to_rgb(cairo_surface_t *const cs, const int w, const int h, uint8_t *out)
{
	const unsigned char *data = cairo_image_surface_get_data(cs);
	const uint32_t *in = (const uint32_t *)data;

	size_t n = w * h;
	for(size_t i=0; i<n; i++) {
		uint32_t temp = *in++;
		*out++ = temp >> 16;
		*out++ = temp >> 8;
		*out++ = temp;
	}
}

void cairo_calc_text_width(cairo_t *const cr, const std::string & font, const int font_height, const std::string & text, int *const width, int *const height)
{
	cairo_select_font_face(cr, font.c_str(), CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, font_height);

	cairo_text_extents_t extents;
	cairo_text_extents(cr, text.c_str(), &extents);

	*width = extents.width;
	*height = font_height;
}

void cairo_draw_text(cairo_t *const cr, const std::string & font, const int font_height, const double r, const double g, const double b, const std::string & str, const int x, const int y)
{
	cairo_set_source_rgb(cr, r, g, b);
	cairo_select_font_face(cr, font.c_str(), CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size(cr, font_height);
	cairo_move_to(cr, x, y);
	cairo_show_text(cr, str.c_str());
	cairo_stroke(cr);
}

void cairo_draw_line(cairo_t *const cr, const int x1, const int y1, const int x2, const int y2, const double r, const double g, const double b)
{
	cairo_set_source_rgb(cr, r, g, b);
	cairo_set_line_width(cr, 1);
	cairo_move_to(cr, x1, y1);
	cairo_line_to(cr, x2, y2);
	cairo_stroke(cr);
}

void cairo_draw_dot(cairo_t *const cr, const int x, const int y, const double radius, const double r, const double g, const double b)
{
	cairo_set_line_width(cr, radius);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	cairo_move_to(cr, x, y);
	cairo_line_to(cr, x, y);
	cairo_stroke(cr);
}

std::string shorten(double value) {
	double chk = fabs(value);

	double divider = 1.0;

	std::string si;
	if (chk >= 1000000000000.0) {
		si = "T";
		divider = 1000000000000.0;
	}
	else if (chk >= 1000000000.0) {
		si = "G";
		divider = 1000000000.0;
	}
	else if (chk >= 1000000.0) {
		si = "M";
		divider = 1000000.0;
	}
	else if (chk >= 1000.0) {
		si = "k";
		divider = 1000.0;
	}
	else if (chk == 0.0)
		return "0";
	else if (chk <= 0.000001) {
		si = "u";
		divider = 0.000001;
	}
	else if (chk <= 0.001) {
		si = "m";
		divider = 0.001;
	}

	std::string dummy = myformat("%.0f", value / divider);
	int len = dummy.length();
	if (len < 3) {
		std::string fmt = "%." + myformat("%d", 3 - len) + "f";
		dummy = myformat(fmt.c_str(), value / divider);
	}

	return dummy + si;
}

void draw_histogram(const std::string & font, const int x_off, const int y_off, const int width, const int height, std::vector<std::string> & labels_x, const double *const val_y, const int n_values, const std::string & metaStr, FILE *const fh)
{
	cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, width, height);
	cairo_t *cr = cairo_create(surface);

	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_paint(cr);

	double y_top = y_off + height;

	double yAxisTop = 12.0;
	double yAxisBottom = height - 12.5;
	int yTicks = 10;
	double xAxisLeft = -1.0;
	double xAxisRight = width - 5.0;
	double font_height = 10.0;

	double black_r = 0.0, black_g = 0.0, black_b = 0.0;
	double gray_r = 0.5, gray_g = 0.5, gray_b = 0.5;
	// double white_r = 1.0, white_g = 1.0, white_b = 1.0;
	double red_r = 1.0, red_g = 0.0, red_b = 0.0;
	// double green_r = 0.0, green_g = 1.0, green_b = 0.0;
	// double yellow_r = 1.0, yellow_g = 1.0, yellow_b = 0.0;
	// double dark_yellow_r = 0.75, dark_yellow_g = 0.75, dark_yellow_b = 0.0;
	// double magenta_r = 1.0, magenta_g = 0.0, magenta_b = 1.0;

	double yMin = 99999999999.9;
	double yMax = -99999999999.9;
	for(int index=0; index<n_values; index++)
	{
		if (val_y[index] < yMin) yMin = val_y[index];
		if (val_y[index] > yMax) yMax = val_y[index];
	}

	// determine x-position of y-axis
	std::string use_width = "9999W";
	if (yMin < 0)
		use_width = "-9999W";
	int valWidth = -1.0, dummy;
	cairo_calc_text_width(cr, font, font_height, use_width, &valWidth, &dummy);
	xAxisLeft = valWidth + 1; // 1 pixel extra space between text and lines

	double hist_w = (xAxisRight - xAxisLeft) / double(n_values);

	int metaWidth = -1;
	cairo_calc_text_width(cr, font, 8.0, metaStr, &metaWidth, &dummy);
	double metaX = width / 2.0 - metaWidth / 2;
	cairo_draw_text(cr, font, 8.0, black_r, black_g, black_b, metaStr, x_off + metaX, 8.0);

	double scaleY = (yAxisBottom - yAxisTop) / (yMax - yMin);

	cairo_draw_line(cr, x_off + xAxisLeft, yAxisTop,    x_off + xAxisLeft,  yAxisBottom, black_r, black_g, black_b);
	cairo_draw_line(cr, x_off + xAxisLeft, yAxisBottom, x_off + xAxisRight, yAxisBottom, black_r, black_g, black_b);

	// draw ticks horizonal
	for(int index=0; index<n_values; index++) {
		double x = hist_w * double(index);

		std::string str = labels_x.at(index);
		int str_w = -1.0, str_h = -1.0;
		cairo_calc_text_width(cr, font, font_height, str, &str_w, &str_h);

		double plot_x = x_off + x + hist_w + str_w / 2.0;

		cairo_draw_text(cr, font, font_height, black_r, black_g, black_b, str, plot_x, (yAxisBottom + 12.5));
	}

	// draw ticks vertical
	for(int yti=0; yti<=yTicks; yti++) {
		double y = (yAxisBottom - yAxisTop) * double(yti) / double(yTicks) + yAxisTop;
		cairo_draw_line(cr, x_off + xAxisLeft - 2.0, y, x_off + xAxisLeft, y, black_r, black_g, black_b);

		double value = (yMax - yMin) / double(yTicks) * double(yTicks - yti) + yMin;

		std::string str = shorten(value);

		cairo_draw_line(cr, x_off + xAxisLeft + 1.0, y, x_off + xAxisRight, y, gray_r, gray_g, gray_b);

		cairo_draw_text(cr, font, font_height, black_r, black_g, black_b, str, x_off + 1.0, (y == yAxisTop ? y + 6.0 : y + 3.0));
	}

	// draw data
	if (n_values > 0) {
		for(int index=0; index<n_values; index++) {
			double x = hist_w * double(index);
			double y = scaleY * (val_y[index] - yMin);

			double x_cur_l = xAxisLeft + x_off + x;
			double x_cur_r = xAxisLeft + x_off + x + hist_w;
			double y_cur_top = (yAxisBottom - y);
			double y_cur_bot = (yAxisBottom - 0.0);

			cairo_draw_line(cr, x_cur_l, y_cur_bot, x_cur_l, y_cur_top, red_r, red_g, red_b);
			cairo_draw_line(cr, x_cur_l, y_cur_top, x_cur_r, y_cur_top, red_r, red_g, red_b);
			cairo_draw_line(cr, x_cur_r, y_cur_top, x_cur_r, y_cur_bot, red_r, red_g, red_b);
		}
	}
	else {
		cairo_draw_text(cr, font, font_height * 1.5, red_r, red_g, red_b, "No data", x_off + xAxisLeft + 5.0, (height / 2.0 + yAxisTop / 2.0));
	}

	cairo_destroy(cr);

        uint8_t *const out = (uint8_t *)valloc(width * height * 3);
        cairo_to_rgb(surface, width, height, out);
	write_PNG_file(fh, width, height, out);
	free(out);
}
