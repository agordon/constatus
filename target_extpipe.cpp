#include "config.h"
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "target_extpipe.h"
#include "error.h"
#include "exec.h"
#include "log.h"
#include "picio.h"
#include "utils.h"

target_extpipe::target_extpipe(const std::string & id, source *const s, const std::string & store_path, const std::string & prefix, const int quality, const int max_time, const double interval, const std::vector<filter *> *const filters, const std::string & exec_start, const std::string & exec_cycle, const std::string & exec_end, const std::string & cmd) : target(id, s, store_path, prefix, max_time, interval, filters, exec_start, exec_cycle, exec_end, -1.0), quality(quality), cmd(cmd)
{
}

target_extpipe::~target_extpipe()
{
	stop();
}

// this function is based on https://stackoverflow.com/questions/9465815/rgb-to-yuv420-algorithm-efficiency
static void Bitmap2Yuv420p_calc2(uint8_t *const destination, const uint8_t *const rgb, const size_t width, const size_t height)
{
	const size_t image_size = width * height, w2 = width / 2;
	const uint8_t *p = rgb;
	uint8_t *Y = destination;
	uint8_t *U = Y + image_size;
	uint8_t *V = Y + image_size + image_size / 4;

	for(size_t line = 0; line < height; ++line) {
		if (line & 1) {
			for(size_t x = 0; x < width; x++) {
				uint8_t r = *p++;
				uint8_t g = *p++;
				uint8_t b = *p++;

				*Y++ = ((66*r + 129*g + 25*b) >> 8) + 16;
			}
		}
		else {
			for(size_t x = 0; x < w2; x++) {
				uint8_t r = *p++;
				uint8_t g = *p++;
				uint8_t b = *p++;

				*Y++ = ((66*r + 129*g + 25*b) >> 8) + 16;
				*U++ = ((-38*r + -74*g + 112*b) >> 8) + 128;
				*V++ = ((112*r + -94*g + -18*b) >> 8) + 128;

				r = *p++;
				g = *p++;
				b = *p++;

				*Y++ = ((66*r + 129*g + 25*b) >> 8) + 16;
			}
		}
	}
}

static void put_frame(FILE *fh, const uint8_t *const in, const int w, const int h)
{
	size_t n = w * h + w * h / 2;
	uint8_t *temp = (uint8_t *)malloc(n);

	Bitmap2Yuv420p_calc2(temp, in, w, h);

	fwrite(temp, 1, n, fh);

	free(temp);
}

void target_extpipe::operator()()
{
	set_thread_name("storee_" + prefix);

	s -> register_user();

	time_t cut_ts = time(NULL) + max_time;

	uint64_t prev_ts = 0;
	bool is_start = true;
	std::string name;
	unsigned f_nr = 0;

	FILE *p_fd = NULL;

	uint8_t *prev_frame = NULL;

	bool first = true;
	std::string workCmd = cmd;

	for(;!local_stop_flag;) {
		pauseCheck();

		int w = -1, h = -1;
		uint8_t *work = NULL;
		size_t work_len = 0;
		s -> get_frame(E_RGB, quality, &prev_ts, &w, &h, &work, &work_len);

		if (max_time > 0 && time(NULL) >= cut_ts) {
			log(LL_DEBUG, "new file");

			if (p_fd) {
				pclose(p_fd);
				p_fd = NULL;
			}

			cut_ts = time(NULL) + max_time;
		}

		if (p_fd == NULL) {
			name = gen_filename(store_path, prefix, "", get_us(), f_nr++);

			if (!exec_start.empty() && is_start) {
				exec(exec_start, name);
				is_start = false;
			}
			else if (!exec_cycle.empty()) {
				exec(exec_cycle, name);
			}

			int fps = interval <= 0 ? 25 : std::max(1, int(1.0 / interval));

			if (first) {
				first = false;

				char fps_str[16];
				snprintf(fps_str, sizeof fps_str, "%d", fps);

				workCmd = search_replace(workCmd, "%fps", fps_str);

				char w_str[16], h_str[16];
				snprintf(w_str, sizeof w_str, "%d", w);
				snprintf(h_str, sizeof h_str, "%d", h);
				workCmd = search_replace(workCmd, "%w", w_str);
				workCmd = search_replace(workCmd, "%h", h_str);

				workCmd = search_replace(workCmd, "%f", name);

				log(LL_DEBUG, "Will invoke %s", workCmd.c_str());
			}

			p_fd = exec(workCmd);

			if (pre_record) {
				log(LL_DEBUG_VERBOSE, "Write pre-recorded frames");

				for(frame_t pair : *pre_record) {
					if (pair.e == E_JPEG) {
						unsigned char *temp = NULL;
						int dw = -1, dh = -1;
						if (!read_JPEG_memory(work, work_len, &dw, &dh, &temp))
							log(LL_INFO, "JPEG decode error");
						else {
							put_frame(p_fd, temp, dw, dh);
							free(temp);
						}
					}
					else {
						put_frame(p_fd, pair.data, w, h);
					}

					free(pair.data);
				}

				delete pre_record;
				pre_record = NULL;
			}
		}

		log(LL_DEBUG_VERBOSE, "Write frame");
		apply_filters(filters, prev_frame, work, prev_ts, w, h);

		put_frame(p_fd, work, w, h);

		free(prev_frame);
		prev_frame = work;

		mysleep(interval, &local_stop_flag, s);
	}

	if (p_fd != NULL)
		pclose(p_fd);

	free(prev_frame);

	if (!exec_end.empty())
		exec(exec_end, name);

	s -> unregister_user();
}
