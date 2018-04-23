// (C) 2017-2018 by folkert van heusden, released under AGPL v3.0
#pragma once

#include <atomic>
#include <pthread.h>
#include <stdint.h>
#include <thread>

#include "interface.h"
#include "resize.h"

typedef enum { E_RGB, E_JPEG } encoding_t;

class source : public interface
{
protected:
	int width, height;
	const double max_fps;
	resize *const r;
	const int resize_w, resize_h, loglevel;
	const double timeout;

	pthread_mutex_t lock;
	pthread_cond_t cond;
	uint64_t ts;
	uint8_t *frame_jpeg, *frame_rgb;
	size_t frame_jpeg_len, frame_rgb_len;

	std::atomic_int user_count;

public:
	source(const std::string & id, const std::string & descr, const double max_fps, resize *const r, const int resize_w, const int resize_h, const int loglevel, const double timeout);
	virtual ~source();

	bool get_frame(const encoding_t pe, const int jpeg_quality, uint64_t *ts, int *width, int *height, uint8_t **frame, size_t *frame_len);
	void set_frame(const encoding_t pe, const uint8_t *const data, const size_t size);
	void set_scaled_frame(const uint8_t *const in, const int sourcew, const int sourceh);
	void set_size(const int w, const int h) { width = w; height = h; }
	bool need_scale() const { return resize_h != -1 || resize_w != -1; }

	int get_width() const { return width; }
	int get_height() const { return height; }

	virtual void operator()() = 0;

	void register_user() { user_count++; }
	void unregister_user() { user_count--; }
	bool work_required();
};
