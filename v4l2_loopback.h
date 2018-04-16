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

public:
	v4l2_loopback(const std::string & id, source *const s, const double fps, const std::string & dev, const std::vector<filter *> *const filters);
	virtual ~v4l2_loopback();

	void operator()();
};
