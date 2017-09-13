#ifndef __TARGET_H__
#define __TARGET_H__

#include <string>
#include <thread>
#include <vector>

#include "interface.h"
#include "filter.h"
#include "source.h"

typedef struct
{
	uint64_t ts;
	int w, h;
	uint8_t *data;
	size_t len;
	encoding_t e;
} frame_t;

std::string gen_filename(const std::string & store_path, const std::string & prefix, const std::string & ext, const uint64_t ts, const unsigned f_nr);

class target : public interface
{
protected:
	source *s;
	const std::string store_path, prefix;
	const int max_time;
	const double interval;
	const std::vector<filter *> *const filters;
	const std::string exec_start, exec_cycle, exec_end;
	const int override_fps;

	std::vector<frame_t> *pre_record;

public:
	target(const std::string & id, source *const s, const std::string & store_path, const std::string & prefix, const int max_time, const double interval, const std::vector<filter *> *const filters, const std::string & exec_start, const std::string & exec_cycle, const std::string & exec_end, const int override_fps);
	virtual ~target();

	void start(std::vector<frame_t> *const pre_record);

	virtual void operator()() = 0;
};

#endif
