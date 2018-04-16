#include "target.h"

class target_jpeg : public target
{
private:
	const int quality;

public:
	target_jpeg(const std::string & id, source *const s, const std::string & store_path, const std::string & prefix, const int quality, const int max_time, const double interval, const std::vector<filter *> *const filters, const std::string & exec_start, const std::string & exec_cycle, const std::string & exec_end);
	virtual ~target_jpeg();

	void operator()();
};
