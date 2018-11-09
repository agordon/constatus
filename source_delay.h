#include "source.h"

typedef struct
{
	uint8_t *data;
	size_t len;
	uint64_t ts;
} h_data_t;

class source_delay : public source
{
private:
	std::vector<h_data_t> frames;
	source *const s;
	const int jpeg_quality;
	const int n_frames;
	const double max_fps;

public:
	source_delay(const std::string & id, const std::string & descr, source *const s, const int jpeg_quality, const int n_frames, const double max_fps, resize *const r, const int resize_w, const int resize_h, const int ll, const double timeout, std::vector<filter *> *const filters);
	virtual ~source_delay();

	void get_frame(const uint64_t older_than, uint8_t **const frame, size_t *const frame_len);

	virtual void operator()();
};
