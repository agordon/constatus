#include "target.h"

class target_vnc : public target
{
private:
	int fd;

public:
	target_vnc(const std::string & id, source *const s, const std::string & adapter, const int port, const int max_time, const double interval, const std::vector<filter *> *const filters, const std::string & exec_start, const std::string & exec_end);
	virtual ~target_vnc();

	void operator()();
};
