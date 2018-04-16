// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "error.h"
#include "source.h"
#include "source_v4l.h"
#include "picio.h"
#include "log.h"
#include "utils.h"

#define min(x, y)	((y) < (x) ? (y) : (x))
#define max(x, y)	((y) > (x) ? (y) : (x))

inline unsigned char clamp_to_uchar(int in)
{
	return in > 255 ? 255 : (in < 0 ? 0 : in);
}

void image_yuv420_to_rgb(unsigned char *yuv420, unsigned char *rgb, int width, int height)
{
	int pos = width * height;
	unsigned char *y_ptr = yuv420;
	unsigned char *u_ptr = yuv420 + pos + (pos >> 2);
	unsigned char *v_ptr = yuv420 + pos;
	int wm1 = width - 1;

	for (int y=0; y<height; y += 2)
	{
		int u_pos = y * width >> 2;
		int y_w = y * width;

		for (int x=0; x<width; x += 2)
		{
			int y_pos = y_w + x;

			/*first*/
			int v_pos = u_pos;
			int Y = y_ptr[y_pos];
			int U = ((u_ptr[u_pos] - 127) * 1865970) >> 20;
			int V = ((v_ptr[v_pos] - 127) * 1477914) >> 20;
			int R = V + Y;
			int B = U + Y;
			int G = (Y * 1788871 - R * 533725 - B * 203424) >> 20; //G = 1.706 * Y - 0.509 * R - 0.194 * B
			*rgb++ = clamp_to_uchar(R);
			*rgb++ = clamp_to_uchar(G);
			*rgb++ = clamp_to_uchar(B);

			/*second*/
			y_pos++;
			Y = y_ptr[y_pos];
			R = V + Y;
			B = U + Y;
			G = (Y * 1788871 - R * 533725 - B * 203424) >> 20; //G = 1.706 * Y - 0.509 * R - 0.194 * B
			*rgb++ = clamp_to_uchar(R);
			*rgb++ = clamp_to_uchar(G);
			*rgb++ = clamp_to_uchar(B);

			/*third*/
			y_pos += wm1;
			Y = y_ptr[y_pos];
			R = V + Y;
			B = U + Y;
			G = (Y * 1788871 - R * 533725 - B * 203424) >> 20; //G = 1.706 * Y - 0.509 * R - 0.194 * B
			*rgb++ = clamp_to_uchar(R);
			*rgb++ = clamp_to_uchar(G);
			*rgb++ = clamp_to_uchar(B);

			/*fourth*/
			y_pos++;
			Y = y_ptr[y_pos];
			R = V + Y;
			B = U + Y;
			G = (Y * 1788871 - R * 533725 - B * 203424) >> 20; //G = 1.706 * Y - 0.509 * R - 0.194 * B
			*rgb++ = clamp_to_uchar(R);
			*rgb++ = clamp_to_uchar(G);
			*rgb++ = clamp_to_uchar(B);

			u_pos++;
		}
	}
}

inline uint8_t clamp_yuyv2(const int v)
{
	return v < 0 ? 0 : (v > 65535 ? 255 : (v >> 8));
}

void image_yuyv2_to_rgb(const unsigned char *__restrict src, const int width, const int height, unsigned char *__restrict dst)
{
	int len = width * height * 2;
	const unsigned char *const end = &src[len];

	for(; src != end;)
	{
		int C = 298 * (*src++ - 16);
		int D = *src++ - 128;
		int C2 = 298 * (*src++ - 16);
		int E = *src++ - 128;

		int Ex = E * 409 + 128;

		*dst++ = clamp_yuyv2(C + Ex);
		int Dx = D * 100 + E * 208 - 128;
		*dst++ = clamp_yuyv2(C - Dx);
		int D1 = 516 * D + 128;
		*dst++ = clamp_yuyv2(C + D1);

		*dst++ = clamp_yuyv2(C2 + Ex);
		*dst++ = clamp_yuyv2(C2 - Dx);
		*dst++ = clamp_yuyv2(C2 + D1);
	}
}

bool source_v4l::try_v4l_configuration(int fd, int *width, int *height, unsigned int *format)
{
	unsigned int prev = *format;

	// determine pixel format: rgb/yuv420/etc
	struct v4l2_format fmt;
	memset(&fmt, 0x00, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd, VIDIOC_G_FMT, &fmt) == -1)
		error_exit(true, "ioctl(VIDIOC_G_FMT) failed");

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = *width;
	fmt.fmt.pix.height = *height;
	fmt.fmt.pix.pixelformat = *format;

	if (ioctl(fd, VIDIOC_S_FMT, &fmt) == -1)
		return false;

	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (ioctl(fd, VIDIOC_G_FMT, &fmt) == -1)
		error_exit(true, "ioctl(VIDIOC_G_FMT) failed");

	*width = fmt.fmt.pix.width;
	*height = fmt.fmt.pix.height;

	*width = fmt.fmt.pix.width;
	*height = fmt.fmt.pix.height;

	char buffer[5] = { 0 };
	memcpy(buffer, &fmt.fmt.pix.pixelformat, 4);
	log(LL_DEBUG, "available format: %dx%d %s", *width, *height, buffer);

	if (fmt.fmt.pix.pixelformat != prev)
		return false;

	return true;
}

source_v4l::source_v4l(const std::string & id, const std::string & dev, const bool prefer_jpeg_in, const bool rpi_workaround_in, const int jpeg_quality, const double max_fps, const int w_override, const int h_override, resize *const r, const int resize_w, const int resize_h, const int loglevel, const double timeout) : source(id, max_fps, r, resize_w, resize_h, loglevel, timeout), prefer_jpeg(prefer_jpeg_in), rpi_workaround(rpi_workaround_in)
{
	fd = open(dev.c_str(), O_RDWR);
	if (fd == -1)
		error_exit(true, "Cannot access video4linux device '%s'", dev.c_str());

	d = dev;

	// verify that it is a capture device
	struct v4l2_capability cap;
	memset(&cap, 0x00, sizeof(cap));

	if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1)
		error_exit(true, "Cannot VIDIOC_QUERYCAP on %s", dev.c_str());

	if ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0)
		error_exit(false, "Device %s can't capture video frames", dev.c_str());

	// set capture resolution
	bool ok = false;
	pixelformat = -1;

	width = w_override;
	height = h_override;

	if (prefer_jpeg) {
		pixelformat = V4L2_PIX_FMT_JPEG;
		if (try_v4l_configuration(fd, &width, &height, &pixelformat))
			ok = true;

		if (!ok) {
			pixelformat = V4L2_PIX_FMT_MJPEG;
			ok = try_v4l_configuration(fd, &width, &height, &pixelformat);
		}
	}
	else {
		pixelformat = V4L2_PIX_FMT_YUYV;
		ok = try_v4l_configuration(fd, &width, &height, &pixelformat);

		if (!ok) {
			pixelformat = V4L2_PIX_FMT_RGB24;
			ok = try_v4l_configuration(fd, &width, &height, &pixelformat);
		}
	}

	if (!ok) {
		if (prefer_jpeg)
			error_exit(true, "Selected pixel format not available (try setting prefer-jpeg to false)");

		error_exit(true, "Selected pixel format not available");
	}

	if (pixelformat == V4L2_PIX_FMT_JPEG) {
#ifdef V4L2_CID_JPEG_COMPRESSION_QUALITY
		struct v4l2_control par = { 0 };
		par.id = V4L2_CID_JPEG_COMPRESSION_QUALITY;
		par.value = jpeg_quality;

		if (ioctl(fd, VIDIOC_S_CTRL, &par) == -1)
			error_exit(true, "ioctl(VIDIOC_S_CTRL) failed");
#else
		struct v4l2_jpegcompression par = { 0 };

		if (ioctl(fd, VIDIOC_G_JPEGCOMP, &par) == -1)
			error_exit(true, "ioctl(VIDIOC_G_JPEGCOMP) failed");

		par.quality = jpeg_quality;

		if (ioctl(fd, VIDIOC_S_JPEGCOMP, &par) == -1)
			error_exit(true, "ioctl(VIDIOC_S_JPEGCOMP) failed");
#endif
	}

	char buffer[5] = { 0 };
	memcpy(buffer, &pixelformat, 4);
	log(LL_INFO, "chosen: %dx%d %s", width, height, buffer);

	// set how we retrieve data (using mmaped thingy)
	struct v4l2_requestbuffers req;
	memset(&req, 0x00, sizeof(req));
	req.count = 1;
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;
	if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1)
		error_exit(true, "ioctl(VIDIOC_REQBUFS) failed");

	struct v4l2_buffer buf;
	memset(&buf, 0x00, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = 0;
	if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1)
		error_exit(true, "ioctl(VIDIOC_QUERYBUF) failed");

	if (ioctl(fd, VIDIOC_QBUF, &buf) == -1)
		error_exit(true, "ioctl(VIDIOC_QBUF) failed");

	enum v4l2_buf_type buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd, VIDIOC_STREAMON, &buf_type) == -1)
		error_exit(true, "ioctl(VIDIOC_STREAMON) failed");

	///// raspberry pi workaround
	// with 3.10.27+ kernel: image would fade to black due
	// to exposure bug
	if (rpi_workaround) {
		int sw = 1;
		if (ioctl(fd, VIDIOC_OVERLAY, &sw) == -1)
			error_exit(true, "ioctl(VIDIOC_OVERLAY) failed");
	}

	unmap_size = buf.length;
	io_buffer = static_cast<uint8_t *>(mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset));

	vw = width;
	vh = height;

	if (need_scale()) {
		width = resize_w;
		height = resize_h;
	}

	th = NULL;
}

source_v4l::~source_v4l()
{
	stop();

	close(fd);

	munmap(io_buffer, unmap_size);
}

void source_v4l::operator()()
{
	log(LL_INFO, "source v4l2 thread started");

	set_thread_name("src_v4l2");

	int bytes = vw * vh * 3;
	unsigned char *conv_buffer = static_cast<unsigned char *>(valloc(bytes));

	struct v4l2_buffer buf = { 0 };
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	const uint64_t interval = max_fps > 0.0 ? 1.0 / max_fps * 1000.0 * 1000.0 : 0;

	for(;!local_stop_flag;)
	{
		time_t start_ts = get_us();

		if (ioctl(fd, VIDIOC_DQBUF, &buf) == -1) {
			log(LL_ERR, "VIDIOC_DQBUF failed: %s", strerror(errno));
		}
		else if (work_required() && !is_paused()) {
			if (prefer_jpeg) {
				int cur_n_bytes = buf.bytesused;

				if (resize_h != -1 || resize_w != -1) {
					int dw = -1, dh = -1;
					unsigned char *temp = NULL;
					if (read_JPEG_memory(io_buffer, cur_n_bytes, &dw, &dh, &temp))
						set_scaled_frame(temp, dw, dh);
					free(temp);
				}
				else {
					set_frame(E_JPEG, io_buffer, cur_n_bytes);
				}
			}
			else {
				if (pixelformat == V4L2_PIX_FMT_YUV420)
					image_yuv420_to_rgb(io_buffer, conv_buffer, vw, vh);
				else if (pixelformat == V4L2_PIX_FMT_YUYV)
					image_yuyv2_to_rgb(io_buffer, vw, vh, conv_buffer);
				else if (pixelformat == V4L2_PIX_FMT_RGB24)
					memcpy(conv_buffer, io_buffer, vw * vh * 3);
				else
					error_exit(false, "video4linux: video source has unsupported pixel format");

				if (need_scale())
					set_scaled_frame(conv_buffer, vw, vh);
				else
					set_frame(E_RGB, conv_buffer, vw * vh * 3);
			}
		}

		if (ioctl(fd, VIDIOC_QBUF, &buf) == -1)
			error_exit(true, "ioctl(VIDIOC_QBUF) failed");

		uint64_t end_ts = get_us();
		int64_t left = interval - (end_ts - start_ts);

		if (interval > 0 && left > 0)
			usleep(left);
	}

	free(conv_buffer);

	log(LL_INFO, "source v4l2 thread terminating");
}
