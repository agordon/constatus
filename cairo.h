// (C) 2017-2018 by folkert van heusden, released under AGPL v3.0
#include <stdint.h>
#include <string>
#include <vector>
#include <cairo/cairo.h>

cairo_surface_t * rgb_to_cairo(const uint8_t *const in, const int w, const int h, uint32_t **temp);
void cairo_to_rgb(cairo_surface_t *const cs, const int w, const int h, uint8_t *out);
void cairo_get_text_width(cairo_t *const cr, const std::string & text, const int font_height, int *const width, int *const height);
void cairo_draw_text(cairo_t *const cr, const std::string & font, const int font_height, const double r, const double g, const double b, const std::string & str, const int x, const int y);
void cairo_draw_line(cairo_t *const cr, const int x1, const int y1, const int x2, const int y2, const double r, const double g, const double b);
void cairo_draw_dot(cairo_t *const cr, const int x, const int y, const double radius, const double r, const double g, const double b);
void draw_histogram(const std::string & font, const int x_off, const int y_off, const int width, const int height, std::vector<std::string> & labels_x, const double *const val_y, const int n_values, const std::string & metaStr, FILE *const fh);
