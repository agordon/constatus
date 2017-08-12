// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <stdio.h>
#include <unistd.h>

#include "error.h"
#include "http_client.h"
#include "source.h"
#include "source_http_jpeg.h"
#include "picio.h"
#include "filter.h"

source_http_jpeg::source_http_jpeg(const std::string & urlIn, const bool ignoreCertIn, const std::string & authIn, std::atomic_bool *const global_stopflag, const int resize_w, const int resize_h) : source(global_stopflag, resize_w, resize_h), url(urlIn), auth(authIn), ignore_cert(ignoreCertIn)
{
	th = new std::thread(std::ref(*this));
}

source_http_jpeg::~source_http_jpeg()
{
	th -> join();
	delete th;
}

void source_http_jpeg::operator()()
{
	bool first = true, resize = resize_h != -1 || resize_w != -1;

	for(;!*global_stopflag;)
	{
		uint8_t *work = NULL;
		size_t work_len = 0;

		if (!http_get(url, ignore_cert, auth.empty() ? NULL : auth.c_str(), &work, &work_len))
		{
			printf("did not get a frame\n");
			usleep(101000);
			continue;
		}

		unsigned char *temp = NULL;
		int dw = -1, dh = -1;
		if (first || resize) {
                        read_JPEG_memory(work, work_len, &dw, &dh, &temp);

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

		free(work);
	}
}
