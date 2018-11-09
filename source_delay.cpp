#include <string.h>
#include <unistd.h>

#include "source_delay.h"
#include "log.h"
#include "utils.h"

source_delay::source_delay(const std::string & id, const std::string & descr, source *const s, const int jpeg_quality, const int n_frames, const double max_fps, resize *const r, const int resize_w, const int resize_h, const int ll, const double timeout, std::vector<filter *> *const filters) : source(id, descr, max_fps, r, resize_w, resize_h, ll, timeout, filters), s(s), jpeg_quality(jpeg_quality), n_frames(n_frames), max_fps(max_fps)
{
}

source_delay::~source_delay()
{
	for(h_data_t hd : frames)
		delete hd.data;
}

void source_delay::operator()()
{
	log(id, LL_INFO, "source_delay thread started");

	set_thread_name("source_delay");

	const uint64_t interval = max_fps > 0.0 ? 1.0 / max_fps * 1000.0 * 1000.0 : 0;

	s -> register_user();

	for(;!local_stop_flag;)
	{
		time_t start_ts = get_us();

		uint64_t ts = 0;
		h_data_t hd;
		if (s -> get_frame(E_JPEG, jpeg_quality, &ts, &width, &height, &hd.data, &hd.len)) {
			while(frames.size() >= n_frames) {
				delete frames.front().data;
				frames.erase(frames.begin());
			}

			frames.push_back(hd);

			set_frame(E_JPEG, frames.front().data, frames.front().len);
		}

		uint64_t end_ts = get_us();
		int64_t left = interval - (end_ts - start_ts);

		if (interval > 0 && left > 0)
			usleep(left);
	}

	s -> unregister_user();

	log(id, LL_INFO, "source_delay thread terminating");
}

void source_delay::get_frame(const uint64_t older_than, uint8_t **const frame, size_t *const frame_len)
{
	h_data_t hdbest = { NULL, 0, 0 };
	int64_t best = 1234567890;

	for(h_data_t & hd : frames) {
		if (hd.ts > older_than)
			continue;
	
		int64_t td = older_than - hd.ts;
		if (td < best) {
			best = td;
			hdbest = hd;
		}
	}

	if (hdbest.data) {
		*frame = (uint8_t *)valloc(hdbest.len);
		memcpy(*frame, hdbest.data, hdbest.len);
		*frame_len = hdbest.len;
	}
	else {
		*frame = NULL;
		*frame_len = 0;
	}
}
