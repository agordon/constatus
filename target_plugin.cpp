#include <unistd.h>

#include "target_plugin.h"
#include "error.h"
#include "exec.h"
#include "log.h"
#include "picio.h"
#include "utils.h"

target_plugin::target_plugin(const std::string & id, source *const s, const std::string & store_path, const std::string & prefix, const int quality, const int max_time, const double interval, const std::vector<filter *> *const filters, const std::string & exec_start, const std::string & exec_cycle, const std::string & exec_end, stream_plugin_t *const sp, const int override_fps) : target(id, s, store_path, prefix, max_time, interval, filters, exec_start, exec_cycle, exec_end, override_fps), quality(quality), sp(sp)
{
}

target_plugin::~target_plugin()
{
	stop();
}

void target_plugin::operator()()
{
	set_thread_name("storep_" + prefix);

	sp -> arg = sp -> init_plugin(sp -> par.c_str());

	s -> register_user();

	time_t cut_ts = time(NULL) + max_time;

	uint64_t prev_ts = 0;
	bool is_start = true, is_open = false;
	std::string name;

	uint8_t *prev_frame = NULL;

	for(;!local_stop_flag;) {
		pauseCheck();

		int w = -1, h = -1;
		uint8_t *work = NULL;
		size_t work_len = 0;
		s -> get_frame(E_RGB, quality, &prev_ts, &w, &h, &work, &work_len);

		if (max_time > 0 && time(NULL) >= cut_ts) {
			log(LL_DEBUG, "new file");

			sp -> close_file(sp -> arg);
			is_open = false;

			cut_ts = time(NULL) + max_time;
		}

		if (!is_open) {
			double fps = interval <= 0 ? 25.0 : (1.0 / interval);

			sp -> open_file(sp -> arg, (store_path + prefix).c_str(), override_fps >= 1 ? override_fps : fps, quality);
			is_open = true;

			if (!exec_start.empty() && is_start) {
				exec(exec_start, name);
				is_start = false;
			}
			else if (!exec_cycle.empty()) {
				exec(exec_cycle, name);
			}

			if (pre_record) {
				log(LL_DEBUG_VERBOSE, "Write pre-recorded frames");
				for(frame_t pair : *pre_record) {
					sp -> write_frame(sp -> arg, pair.ts, w, h, prev_frame, pair.data);

					free(pair.data);
				}

				delete pre_record;
				pre_record = NULL;
			}
		}

		apply_filters(filters, prev_frame, work, prev_ts, w, h);

		log(LL_DEBUG_VERBOSE, "Write frame");
		sp -> write_frame(sp -> arg, prev_ts, w, h, prev_frame, work);

		free(prev_frame);
		prev_frame = work;

		mysleep(interval, &local_stop_flag, s);
	}

	if (is_open)
		sp -> close_file(sp -> arg);

	free(prev_frame);

	if (!exec_end.empty())
		exec(exec_end, name);

	sp -> uninit_plugin(sp -> arg);

	s -> unregister_user();
}
