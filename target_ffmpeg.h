#include "target.h"

class target_ffmpeg : public target
{
private:
	const std::string parameters, type;
	unsigned bitrate;

public:
	target_ffmpeg(const std::string & id, const std::string & parameters, source *const s, const std::string & store_path, const std::string & prefix, const int max_time, const double interval, const std::string & type, const int bitrate, const std::vector<filter *> *const filters, const std::string & exec_start, const std::string & exec_cycle, const std::string & exec_end, const int override_fps);
	virtual ~target_ffmpeg();

	void operator()();
};
