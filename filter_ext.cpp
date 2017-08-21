// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <dlfcn.h>
#include <string.h>

#include "error.h"
#include "filter_ext.h"

filter_ext::filter_ext(const std::string & filter_filename, const std::string & parameter)
{
	library = dlopen(filter_filename.c_str(), RTLD_NOW);
	if (!library)
		error_exit(true, "Failed opening filter plugin library %s", filter_filename.c_str());

	init_filter = (init_filter_t)dlsym(library, "init_filter");
	if (!init_filter)
		error_exit(true, "Failed finding filter plugin \"init_filter\" in %s", filter_filename.c_str());

	apply_filter = (apply_filter_t)dlsym(library, "apply_filter");
	if (!apply_filter)
		error_exit(true, "Failed finding filter plugin \"apply_filter\" in %s", filter_filename.c_str());

	arg = init_filter(parameter.c_str());
}

filter_ext::~filter_ext()
{
	dlclose(library);
}

void filter_ext::apply(const uint64_t ts, const int w, const int h, const uint8_t *const prev, const uint8_t *const in, uint8_t *const out)
{
	apply_filter(arg, ts, w, h, prev, in, out);
}
