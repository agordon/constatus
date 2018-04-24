// (C) 2017-2018 by folkert van heusden, released under AGPL v3.0
#include <string>
#include <thread>
#include <vector>

#include "interface.h"
#include "filter.h"
#include "source.h"

class v4l2_loopback : public interface
{
private:
	source *const s;
	const double fps;
	const std::string dev;
	const std::vector<filter *> *const filters;
	instance_t *const inst;

public:
	v4l2_loopback(const std::string & id, const std::string & descr, source *const s, const double fps, const std::string & dev, const std::vector<filter *> *const filters, instance_t *const inst);
	virtual ~v4l2_loopback();

	void operator()();
};
