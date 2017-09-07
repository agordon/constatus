#include "target.h"

class target_ffmpeg : public target
{
private:
	unsigned bitrate;

public:
	target_ffmpeg(const std::string & id, source *const s, const std::string & store_path, const std::string & prefix, const int max_time, const double interval, const int bitrate, const std::vector<filter *> *const filters, const char *const exec_start, const char *const exec_cycle, const char *const exec_end);
	virtual ~target_ffmpeg();

	void operator()();
};
