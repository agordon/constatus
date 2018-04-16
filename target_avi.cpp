#include "config.h"
#ifdef WITH_GWAVI
extern "C" {
#include <gwavi.h>
}
#include <unistd.h>

#include "target_avi.h"
#include "error.h"
#include "exec.h"
#include "log.h"
#include "picio.h"
#include "utils.h"

target_avi::target_avi(const std::string & id, source *const s, const std::string & store_path, const std::string & prefix, const int quality, const int max_time, const double interval, const std::vector<filter *> *const filters, const std::string & exec_start, const std::string & exec_cycle, const std::string & exec_end, const int override_fps) : target(id, s, store_path, prefix, max_time, interval, filters, exec_start, exec_cycle, exec_end, override_fps), quality(quality)
{
}

target_avi::~target_avi()
{
	stop();
}

void target_avi::operator()()
{
	set_thread_name("storea_" + prefix);

	s -> register_user();

	time_t cut_ts = time(NULL) + max_time;

	uint64_t prev_ts = 0;
	bool is_start = true;
	std::string name;
	unsigned f_nr = 0;

	uint8_t *prev_frame = NULL;

	struct gwavi_t *gwavi = NULL;

	for(;!local_stop_flag;) {
		pauseCheck();

		int w = -1, h = -1;
		uint8_t *work = NULL;
		size_t work_len = 0;
		s -> get_frame(filters -> empty() ? E_JPEG : E_RGB, quality, &prev_ts, &w, &h, &work, &work_len);

		if (max_time > 0 && time(NULL) >= cut_ts) {
			log(LL_DEBUG, "new file");

			if (gwavi)
				gwavi_close(gwavi);
			gwavi = NULL;

			cut_ts = time(NULL) + max_time;
		}

		if (!gwavi) {
			name = gen_filename(store_path, prefix, "avi", get_us(), f_nr++);

			if (!exec_start.empty() && is_start) {
				exec(exec_start, name);
				is_start = false;
			}
			else if (!exec_cycle.empty()) {
				exec(exec_cycle, name);
			}

			int fps = interval <= 0 ? 25 : std::max(1, int(1.0 / interval));

			gwavi = gwavi_open((char *)name.c_str(), w, h, (char *)"MJPG", override_fps >= 1 ? override_fps : fps, NULL);
			if (!gwavi)
				error_exit(true, "Cannot create %s", name.c_str());

			if (pre_record) {
				log(LL_DEBUG_VERBOSE, "Write pre-recorded frames");

				for(frame_t pair : *pre_record) {
					if (pair.e == E_JPEG)
						gwavi_add_frame(gwavi, pair.data, pair.len);
					else {
						char *data = NULL;
						size_t data_size = 0;
						write_JPEG_memory(s -> get_meta(), w, h, quality, pair.data, &data, &data_size);

						gwavi_add_frame(gwavi, (unsigned char *)data, data_size);

						free(data);
					}

					free(pair.data);
				}

				delete pre_record;
				pre_record = NULL;
			}
		}

		log(LL_DEBUG_VERBOSE, "Write frame");
		if (filters -> empty())
			gwavi_add_frame(gwavi, work, work_len);
		else {
			apply_filters(filters, prev_frame, work, prev_ts, w, h);

			char *data = NULL;
			size_t data_size = 0;
			write_JPEG_memory(s -> get_meta(), w, h, quality, work, &data, &data_size);
			gwavi_add_frame(gwavi, (unsigned char *)data, data_size);
			free(data);
		}

		free(prev_frame);
		prev_frame = work;

		mysleep(interval, &local_stop_flag, s);
	}

	if (gwavi)
		gwavi_close(gwavi);

	free(prev_frame);

	if (!exec_end.empty())
		exec(exec_end, name);

	s -> unregister_user();
}
#endif
