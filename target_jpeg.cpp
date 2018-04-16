#include <unistd.h>

#include "target_jpeg.h"
#include "error.h"
#include "exec.h"
#include "log.h"
#include "picio.h"
#include "utils.h"

target_jpeg::target_jpeg(const std::string & id, source *const s, const std::string & store_path, const std::string & prefix, const int quality, const int max_time, const double interval, const std::vector<filter *> *const filters, const std::string & exec_start, const std::string & exec_cycle, const std::string & exec_end) : target(id, s, store_path, prefix, max_time, interval, filters, exec_start, exec_cycle, exec_end, -1), quality(quality)
{
}

target_jpeg::~target_jpeg()
{
	stop();
}

void target_jpeg::operator()()
{
	set_thread_name("storej_" + prefix);

	uint64_t prev_ts = 0;
	bool is_start = true;
	std::string name;

	uint8_t *prev_frame = NULL;

	int f_nr = 0;

	s -> register_user();

	// pre-recorded frames
	if (pre_record) {
		for(frame_t pair : *pre_record) {
			name = gen_filename(store_path, prefix, "jpg", pair.ts, f_nr++);

			FILE *fh = fopen(name.c_str(), "wb");
			if (!fh)
				error_exit(true, "Cannot create file %s", name.c_str());

			if (pair.e == E_JPEG)
				fwrite(pair.data, pair.len, 1, fh);
			else {
				char *data = NULL;
				size_t data_size = 0;
				write_JPEG_memory(s -> get_meta(), pair.w, pair.h, quality, pair.data, &data, &data_size);

				fwrite(data, data_size, 1, fh);

				free(data);
			}

			fclose(fh);

			free(pair.data);
		}

		delete pre_record;
	}

	for(;!local_stop_flag;) {
		pauseCheck();

		int w = -1, h = -1;
		uint8_t *work = NULL;
		size_t work_len = 0;
		s -> get_frame(filters -> empty() ? E_JPEG : E_RGB, quality, &prev_ts, &w, &h, &work, &work_len);

		name = gen_filename(store_path, prefix, "jpg", get_us(), f_nr++);

		if (!exec_start.empty() && is_start) {
			exec(exec_start, name);
			is_start = false;
		}

		log(LL_DEBUG_VERBOSE, "Write frame to %s", name.c_str());
		FILE *fh = fopen(name.c_str(), "wb");
		if (!fh)
			error_exit(true, "Cannot create file %s", name.c_str());

		if (filters -> empty())
			fwrite(work, work_len, 1, fh);
		else {
			apply_filters(filters, prev_frame, work, prev_ts, w, h);

			char *data = NULL;
			size_t data_size = 0;
			write_JPEG_memory(s -> get_meta(), w, h, quality, work, &data, &data_size);
			fwrite(data, data_size, 1, fh);
			free(data);
		}

		fclose(fh);

		free(prev_frame);
		prev_frame = work;

		mysleep(interval, &local_stop_flag, s);
	}

	free(prev_frame);

	if (!exec_end.empty())
		exec(exec_end, name);

	s -> unregister_user();
}
