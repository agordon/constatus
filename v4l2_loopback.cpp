// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <atomic>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include "error.h"
#include "source.h"
#include "utils.h"
#include "filter.h"
#include "log.h"
#include "v4l2_loopback.h"

v4l2_loopback::v4l2_loopback(const std::string & id, source *const s, const double fps, const std::string & dev, const std::vector<filter *> *const filters) : interface(id), s(s), fps(fps), dev(dev), filters(filters)
{
	th = NULL;
	local_stop_flag = false;
	ct = CT_LOOPBACK;
}

v4l2_loopback::~v4l2_loopback()
{
	stop();
	free_filters(filters);
}

void v4l2_loopback::operator()()
{
	set_thread_name("p2vl");

	s -> register_user();

	int v4l2sink = open(dev.c_str(), O_WRONLY);
	if (v4l2sink == -1)
        	error_exit(true, "Failed to open v4l2sink device (%s)", dev.c_str());

	// setup video for proper format
	struct v4l2_format v;
	v.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

	if (ioctl(v4l2sink, VIDIOC_G_FMT, &v))
		error_exit(true, "VIDIOC_G_FMT failed");

	uint8_t *prev_frame = NULL;

	uint64_t prev_ts = 0;
	bool first = true;

	for(;!local_stop_flag;) {
		pauseCheck();

		int w = -1, h = -1;
		uint8_t *work = NULL;
		size_t work_len = 0;
		s -> get_frame(E_RGB, -1, &prev_ts, &w, &h, &work, &work_len);

		if (first) {
			first = false;

			v.fmt.pix.width = w;
			v.fmt.pix.height = h;
			v.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
			v.fmt.pix.sizeimage = w * h * 3;
			if (ioctl(v4l2sink, VIDIOC_S_FMT, &v) == -1)
				error_exit(true, "VIDIOC_S_FMT failed");
		}

		apply_filters(filters, prev_frame, work, prev_ts, w, h);

		if (write(v4l2sink, work, work_len) == -1)
			error_exit(true, "write to video loopback failed");

		free(prev_frame);
		prev_frame = work;

		if (fps > 0)
			mysleep(1.0 / fps, &local_stop_flag, s);
	}

	free(prev_frame);

	s -> unregister_user();

	close(v4l2sink);
}
