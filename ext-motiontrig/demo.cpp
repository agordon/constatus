// (C) 2017 by folkert van heusden, this file is released in the public domain
#include <algorithm>
#include <stdint.h>
#include <string.h>

extern "C" {

void init_motion_trigger(const char *const parameter)
{
	// you can use the parameter for anything you want
	// e.g. the filename of a configuration file or
	// maybe a variable or whatever
}

bool detect_motion(const uint64_t ts, const int w, const int h, const uint8_t *const prev_frame, uint8_t *const current_frame)
{
	return false;
}

}
