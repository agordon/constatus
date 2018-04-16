// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <stdio.h>
#include <unistd.h>

#include "error.h"
#include "http_client.h"
#include "source.h"
#include "source_http_jpeg.h"
#include "picio.h"
#include "filter.h"
#include "log.h"
#include "utils.h"

source_http_jpeg::source_http_jpeg(const std::string & id, const std::string & urlIn, const bool ignoreCertIn, const std::string & authIn, const double max_fps, resize *const r, const int resize_w, const int resize_h, const int ll, const double timeout) : source(id, max_fps, r, resize_w, resize_h, ll, timeout), url(urlIn), auth(authIn), ignore_cert(ignoreCertIn)
{
	d = url;
}

source_http_jpeg::~source_http_jpeg()
{
	stop();
}

void source_http_jpeg::operator()()
{
	log(LL_INFO, "source http jpeg thread started");

	set_thread_name("src_h_jpeg");

	bool first = true, resize = resize_h != -1 || resize_w != -1;

	const uint64_t interval = max_fps > 0.0 ? 1.0 / max_fps * 1000.0 * 1000.0 : 0;
	long int backoff = 101000;

	for(;!local_stop_flag;)
	{
		time_t start_ts = get_us();

		uint8_t *work = NULL;
		size_t work_len = 0;

		if (!http_get(url, ignore_cert, auth.empty() ? NULL : auth.c_str(), loglevel == LL_DEBUG_VERBOSE, &work, &work_len, &local_stop_flag))
		{
			log(LL_INFO, "did not get a frame");
			usleep(backoff);

			if (backoff <= 2000000)
				backoff += 51000;
			continue;
		}

		backoff = 101000;

		if (work_required() && !is_paused()) {
			unsigned char *temp = NULL;
			int dw = -1, dh = -1;
			if (first || resize) {
				if (!read_JPEG_memory(work, work_len, &dw, &dh, &temp)) {
					log(LL_INFO, "JPEG decode error");
					continue;
				}

				if (resize) {
					width = resize_w;
					height = resize_h;
				}
				else {
					width = dw;
					height = dh;
				}

				first = false;
			}

			if (resize)
				set_scaled_frame(temp, dw, dh);
			else
				set_frame(E_JPEG, work, work_len);

			free(temp);
		}

		free(work);

		uint64_t end_ts = get_us();
		int64_t left = interval - (end_ts - start_ts);

		if (interval > 0 && left > 0)
			usleep(left);
	}

	log(LL_INFO, "source http jpeg thread terminating");
}
