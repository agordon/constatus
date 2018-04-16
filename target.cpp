#include <unistd.h>

#include "target.h"
#include "utils.h"
#include "error.h"

std::string gen_filename(const std::string & store_path, const std::string & prefix, const std::string & ext, const uint64_t ts, const unsigned f_nr)
{
	time_t tv_sec = ts / 1000 / 1000;
	uint64_t tv_usec = ts % (1000 * 1000);

	struct tm tm;
	localtime_r(&tv_sec, &tm);

	std::string use_ext = !ext.empty() ? "." + ext : "";

	return myformat("%s/%s%04d-%02d-%02d_%02d:%02d:%02d.%03d-%u%s",
			store_path.c_str(), prefix.c_str(),
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec,
			unsigned(tv_usec / 1000),
			f_nr, use_ext.c_str());
}

target::target(const std::string & id, source *const s, const std::string & store_path, const std::string & prefix, const int max_time, const double interval, const std::vector<filter *> *const filters, const std::string & exec_start, const std::string & exec_cycle, const std::string & exec_end, const int override_fps) : interface(id), s(s), store_path(store_path), prefix(prefix), max_time(max_time), interval(interval), filters(filters), exec_start(exec_start), exec_cycle(exec_cycle), exec_end(exec_end), override_fps(override_fps)
{
	th = NULL;
	local_stop_flag = false;
	pre_record = NULL;
	ct = CT_TARGET;
	d = store_path + " / " + prefix;
}

target::~target()
{
	free_filters(filters);
	delete filters;
}

void target::start(std::vector<frame_t> *const pre_record)
{
	this -> pre_record = pre_record;

	interface::start();
}
