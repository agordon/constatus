// (C) 2017-2018 by folkert van heusden, released under AGPL v3.0
#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "source.h"
#include "error.h"
#include "picio.h"
#include "filter.h"
#include "log.h"
#include "filter_add_text.h"

source::source(const std::string & id, const std::string & descr) : interface(id, descr), max_fps(-1), r(NULL), resize_w(-1), resize_h(-1), loglevel(LL_INFO)/*FIXME*/, timeout(3.0/*FIXME*/), filters(NULL)
{
	frame_rgb = NULL;
	frame_jpeg = NULL;
	cond = PTHREAD_COND_INITIALIZER;
	lock = PTHREAD_MUTEX_INITIALIZER;
}

source::source(const std::string & id, const std::string & descr, const double max_fps, resize *const r, const int resize_w, const int resize_h, const int loglevel, const double timeout, std::vector<filter *> *const filters) : interface(id, descr), max_fps(max_fps), r(r), resize_w(resize_w), resize_h(resize_h), loglevel(loglevel), timeout(timeout), filters(filters)
{
	width = height = -1;
	ts = 0;

	frame_rgb = NULL;
	frame_rgb_len = 0;
	frame_jpeg = NULL;
	frame_jpeg_len = 0;

	cond = PTHREAD_COND_INITIALIZER;

	lock = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutexattr_t ma;
	pthread_mutexattr_init(&ma);
	pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_ERRORCHECK);
	pthread_mutex_init(&lock, &ma);

	user_count = 0;
	ct = CT_SOURCE;
}

source::~source()
{
	free(frame_rgb);
	free(frame_jpeg);
}

bool source::work_required()
{
//	int t = user_count;
//	printf("%d\n", t);

	return user_count > 0;
}

void source::set_frame(const encoding_t pe_in, const uint8_t *const data, const size_t size)
{
	if (data == NULL || size == 0)
		return;

	if (pthread_mutex_lock(&lock) != 0)
		error_exit(false, "pthread_mutex_lock failed (source::set_frame)");

	encoding_t pe = pe_in;

	struct timespec tv;
	clock_gettime(CLOCK_REALTIME, &tv);
	ts = uint64_t(tv.tv_sec) * uint64_t(1000 * 1000) + uint64_t(tv.tv_nsec / 1000);

	if (pe != E_RGB) {
		free(frame_rgb);
		frame_rgb = NULL;
	}

	if (pe != E_JPEG) {
		free(frame_jpeg);
		frame_jpeg = NULL;
	}

	if (size) {
		if (pe == E_RGB) {
			if (size != frame_rgb_len) {
				free(frame_rgb);

				frame_rgb = (uint8_t *)valloc(size);
				frame_rgb_len = size;
			}

			memcpy(frame_rgb, data, size);
		}
		else {
			if (size != frame_jpeg_len) {
				free(frame_jpeg);

				frame_jpeg = (uint8_t *)valloc(size);
				frame_jpeg_len = size;
			}

			memcpy(frame_jpeg, data, size);
		}

		// FIXME filters
		if (filters && !filters -> empty()) {
			if (pe != E_RGB) {
				if (!read_JPEG_memory(frame_jpeg, frame_jpeg_len, &width, &height, &frame_rgb))
					return;

				frame_rgb_len = width * height * 3;

				pe = E_RGB;
			}

			// FIXME
			apply_filters(NULL, this, filters, NULL, frame_rgb, ts, width, height);
		}
	}

	pthread_cond_broadcast(&cond);

	pthread_mutex_unlock(&lock);
}

bool source::get_frame(const encoding_t pe, const int jpeg_quality, uint64_t *ts, int *width, int *height, uint8_t **frame, size_t *frame_len)
{
	struct timespec spec;
	clock_gettime(CLOCK_REALTIME, &spec);

	time_t to_s = spec.tv_sec + timeout;
	long to_ns = spec.tv_nsec + (timeout - time_t(timeout)) * 1000l * 1000l * 1000l;

	to_s += to_ns / 1000000000l;
	to_ns %= 1000000000l;

	if (timeout <= 0) {
		to_s = spec.tv_sec + 1;
		to_ns = spec.tv_nsec;
	}

	struct timespec tc = { to_s, to_ns };

	bool err = false;

	*frame = NULL;
	*frame_len = 0;

	if (pthread_mutex_lock(&lock) != 0)
		error_exit(false, "pthread_mutex_lock failed (source::get_frame)");

	while(this -> ts <= *ts) {
		if (pthread_cond_timedwait(&cond, &lock, &tc) == ETIMEDOUT) {
			err = true;
			break;
		}
	}

	if (err || (!frame_rgb && !frame_jpeg)) {
fail:
		log(id, LL_INFO, "frame fail %d %p %p | %" PRIu64 " %" PRIu64 " > %" PRId64 " | %f", err, frame_rgb, frame_jpeg, this -> ts, *ts, this -> ts - *ts, timeout);

		if (this -> width <= 0) {
			if (this -> resize_w != -1) {
				*width = this -> resize_w;
				*height = this -> resize_h;
			}
			else {
				*width = 352;
				*height = 288;
			}
		}
		else {
			*width = this -> width;
			*height = this -> height;
		}

		pthread_mutex_unlock(&lock);

		size_t bytes = *width * *height * 3;
		uint8_t *fail = (uint8_t *)valloc(bytes);
		memset(fail, 0x80, bytes);

		filter_add_text fat("Camera down since %c", center_center);
		fat.apply(NULL, this, this -> ts, *width, *height, NULL, fail);

		if (pe == E_RGB) {
			*frame = fail;
			*frame_len = bytes;
			fail = NULL;
		}
		else {
			write_JPEG_memory(get_meta(), *width, *height, jpeg_quality, fail, (char **)frame, frame_len);
		}

		free(fail);

		return false;
	}

	*ts = this -> ts;

	*width = this -> width;
	*height = this -> height;

	if (pe == E_RGB) {
		if (!frame_rgb) {
			if (!read_JPEG_memory(frame_jpeg, frame_jpeg_len, width, height, &frame_rgb))
				goto fail;

			frame_rgb_len = *width * *height * 3;
		}

		*frame = (uint8_t *)valloc(frame_rgb_len);
		memcpy(*frame, frame_rgb, frame_rgb_len);
		*frame_len = frame_rgb_len;
	}
	else { // jpeg
		if (!frame_jpeg)
			write_JPEG_memory(get_meta(), *width, *height, jpeg_quality, frame_rgb, (char **)&frame_jpeg, &frame_jpeg_len);

		*frame = (uint8_t *)valloc(frame_jpeg_len);
		memcpy(*frame, frame_jpeg, frame_jpeg_len);
		*frame_len = frame_jpeg_len;
	}

	pthread_mutex_unlock(&lock);

	return true;
}

void source::set_scaled_frame(const uint8_t *const in, const int sourcew, const int sourceh)
{
	int target_w = resize_w != -1 ? resize_w : sourcew;
	int target_h = resize_h != -1 ? resize_h : sourceh;

	//printf("%dx%d => %dx%d\n", sourcew, sourceh, target_w, target_h);
	uint8_t *out = NULL;
	r -> do_resize(sourcew, sourceh, in, target_w, target_h, &out);

	set_frame(E_RGB, out, target_w * target_h * 3);

	width = target_w;
	height = target_h;

	free(out);
}
