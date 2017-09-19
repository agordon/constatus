#include "config.h"

#include <algorithm>
#include <map>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "target_vnc.h"
#include "error.h"
#include "exec.h"
#include "log.h"
#include "picio.h"
#include "utils.h"

typedef struct
{
	int fd;
	source *s;
	std::atomic_bool *local_stop_flag;
}
vnc_thread_t;

typedef struct {
	int x, y;
	int w, h;
} block_t;

typedef enum { ENC_NONE = 0, ENC_COPY, ENC_RAW, ENC_SOLID_COLOR } enc_t;

typedef struct {
	enc_t method;
	int x, y;
	int w, h;
	int src_x, src_y;
	int r, g, b;
} do_block_t;

typedef struct {
	int red_max, green_max, blue_max;
	int red_bits, green_bits, blue_bits;
	int red_shift, green_shift, blue_shift;
	bool big_endian, true_color;
	int bpp, depth;
} pixel_setup_t;

typedef struct
{
	bool raw;
	bool copy;
	bool hextile;
} enc_allowed_t;

void put_card32(char *p, int v)
{
	uint32_t *P = (uint32_t *)p;

	*P = htonl(v);
}

void put_card16(char *p, int v)
{
	uint16_t *P = (uint16_t *)p;

	*P = htons(v);
}

int get_card16(char *p)
{
	uint16_t *P = (uint16_t *)p;

	return ntohs(*P);
}

int read_card32(int fd)
{
	uint32_t bytes = -1;

	if (READ(fd, (char *)&bytes, sizeof bytes) != sizeof bytes)
		return -1;

	return ntohl(bytes);
}

int read_card16(int fd)
{
	uint16_t bytes = 0;

	if (READ(fd, (char *)&bytes, sizeof bytes) != sizeof bytes)
		return -1;

	return ntohs(bytes);
}

bool handshake(int fd, source *s, pixel_setup_t *ps, int *const w, int *const h)
{
	const char *pv = "RFB 003.007\n";

	if (WRITE(fd, pv, strlen(pv)) == -1)
		return false;

	// ignore requested version
	char pv_reply[12];
	if (READ(fd, pv_reply, 12) == -1)
		return false;

	char sec[2] = { 1, 1 };
	if (WRITE(fd, sec, sizeof sec) == -1)
		return false;

	char secres[1];
	if (READ(fd, secres, 1) == -1)
		return false;

	if (secres[0] != 1)
		return false;

	// wether the server should allow sharing
	char share[1];
	if (READ(fd, share, 1) == -1)
		return false;

	uint64_t prev_ts = 0;
	for(;;) {
		uint8_t *work = NULL;
		size_t work_len = 0;
		if (s -> get_frame(E_RGB, -1, &prev_ts, w, h, &work, &work_len)) {
			free(work);
			break;
		}
	}

	const char name[] = NAME " " VERSION;
	int name_len = sizeof name;
	int si_len = 24 + name_len;
	char *server_init = new char[si_len];
	put_card16(&server_init[0], *w);
	put_card16(&server_init[2], *h);
	server_init[4] = ps -> bpp;	// bits per pixel
	server_init[5] = ps -> depth;	// depth
	server_init[6] = ps -> big_endian;	// big endian
	server_init[7] = ps -> true_color;	// true color
	put_card16(&server_init[8], ps -> red_max);	// red max
	put_card16(&server_init[10], ps -> green_max);	// green max
	put_card16(&server_init[12], ps -> blue_max);	// blue max
	server_init[14] = ps -> red_shift;	// red shift
	server_init[15] = ps -> green_shift;	// green shift
	server_init[16] = ps -> blue_shift;	// blue shift
	server_init[17] = 0;	// padding
	server_init[18] = 0;	// padding
	server_init[19] = 0;	// padding
	put_card32(&server_init[20], name_len);	// name length
	memcpy(&server_init[24], name, name_len);
	if (WRITE(fd, server_init, si_len) != si_len)
		return false;
	delete [] server_init;

	return true;
}

void fill_block(unsigned char *dest, int i_W, int i_H, do_block_t *b)
{
	int W = std::min(b -> w, std::min(i_W - b -> x, i_W - b -> src_x));
	int H = std::min(b -> h, std::min(i_H - b -> y, i_H - b -> src_y));

	for(int y=0; y<H; y++)
	{
		for(int x=0; x<W; x++)
		{
			int dx = x + b -> x, dy = y + b -> y;
			int offset_dest = dy * i_W * 3 + dx * 3;

			dest[offset_dest + 0] = b -> r;
			dest[offset_dest + 1] = b -> g;
			dest[offset_dest + 2] = b -> b;
		}
	}
}

void copy_block(unsigned char *dest, unsigned char *src, int i_W, int i_H, do_block_t *b)
{
	int W = std::min(b -> w, std::min(i_W - b -> x, i_W - b -> src_x));
	int H = std::min(b -> h, std::min(i_H - b -> y, i_H - b -> src_y));

	for(int y=0; y<H; y++)
	{
		for(int x=0; x<W; x++)
		{
			int dx = x + b -> x, dy = y + b -> y;
			int sx = x + b -> src_x, sy = y + b -> src_y;
			int offset_dest = dy * i_W * 3 + dx * 3;
			int offset_src  = sy * i_W * 3 + sx * 3;

			dest[offset_dest + 0] = src[offset_src + 0];
			dest[offset_dest + 1] = src[offset_src + 1];
			dest[offset_dest + 2] = src[offset_src + 2];
		}
	}
}

uint32_t hash_block(unsigned char *img, int i_W, int i_H, int block_x, int block_y, int block_W, int block_H, double fuzzy)
{
	uint32_t val = 0;

	if (fuzzy) {
		for(int y=block_y; y<block_y + block_H; y++) {
			int y_offset = y * i_W * 3;

			for(int x=block_x; x<block_x + block_W; x++) {
				int offset = y_offset + x * 3;

				uint32_t pixel = ((img[offset + 0] + img[offset + 1] + img[offset + 2]) / (3 * fuzzy)) * fuzzy;

				val += pixel;
				val += val << 10;
				val ^= val >> 6;
			}
		}
	}
	else {
		for(int y=block_y; y<block_y + block_H; y++) {
			int y_offset = y * i_W * 3;

			for(int x=block_x; x<block_x + block_W; x++) {
				int offset = y_offset + x * 3;

				uint32_t pixel = (img[offset + 0] << 16) | (img[offset + 1] << 8) | img[offset + 2];

				val += pixel;
				val += val << 10;
				val ^= val >> 6;
			}
		}
	}

	val += val << 3;
	val ^= val >> 11;
	val += val << 15;

	return val;
}

void encode_color(pixel_setup_t *ps, int r, int g, int b, char *to, int *Bpp)
{
	if (ps -> bpp == 32)
		*Bpp = 4;
	else if (ps -> bpp == 24)
		*Bpp = 3;
	else if (ps -> bpp == 16 || ps -> bpp == 15)
		*Bpp = 2;
	else if (ps -> bpp == 8)
		*Bpp = 1;

	char *p = to;

	if (ps -> bpp == 32 || ps -> bpp == 24)
	{
		if (ps -> big_endian)
		{
			*p++ = r;
			*p++ = g;
			*p++ = b;
		}
		else
		{
			*p++ = b;
			*p++ = g;
			*p++ = r;
		}

		if (ps -> bpp == 32)
			*p++ = -1;
	}
	else if (ps -> bpp == 16 || ps -> bpp == 15 || ps -> bpp == 8)
	{
		int dummy = ((r >> (8 - ps -> red_bits)) << ps -> red_shift) |
			((g >> (8 - ps -> green_bits)) << ps -> green_shift) |
			((b >> (8 - ps -> blue_bits)) << ps -> blue_shift);

		if (ps -> bpp == 8)	// 8 bit
		{
			*p = dummy;
			p++;
		}
		else if (ps -> big_endian) // 16/15 bit
		{
			*p = dummy >> 8;
			p++;
			*p = dummy & 255;
			p++;
		}
		else // 16/15 bit
		{
			*p = dummy & 255;
			p++;
			*p = dummy >> 8;
			p++;
		}
	}
}

void create_block_raw(int fd, unsigned char *img, int i_W, int i_H, int block_x, int block_y, int block_w, int block_h, char **out, int *len, pixel_setup_t *ps)
{
	int Bpp = 0;

	if (ps -> bpp == 32)
		Bpp = 4;
	else if (ps -> bpp == 24)
		Bpp = 3;
	else if (ps -> bpp == 16 || ps -> bpp == 15)
		Bpp = 2;
	else if (ps -> bpp == 8)
		Bpp = 1;

	*len = Bpp * block_w * block_h;
	*out = new char[*len];

	char *p = *out;

	if (ps -> bpp == 32)
	{
		for(int y=block_y; y<block_y + block_h; y++)
		{
			const uint8_t *o = &img[y * i_W * 3 + block_x * 3];

			for(int x=block_x; x<block_x + block_w; x++)
			{
				int r = *o++;
				int g = *o++;
				int b = *o++;

				if (ps -> big_endian) {
					*p++ = r;
					*p++ = g;
					*p++ = b;
					*p++ = -1;
				}
				else {
					*p++ = b;
					*p++ = g;
					*p++ = r;
					*p++ = -1;
				}
			}
		}
	}
	else if (ps -> bpp == 24) {
		for(int y=block_y; y<block_y + block_h; y++) {
			const uint8_t *o = &img[y * i_W * 3 + block_x * 3];

			for(int x=block_x; x<block_x + block_w; x++) {
				int r = *o++;
				int g = *o++;
				int b = *o++;

				if (ps -> big_endian) {
					*p++ = r;
					*p++ = g;
					*p++ = b;
				}
				else {
					*p++ = b;
					*p++ = g;
					*p++ = r;
				}
			}
		}
	}
	else if (ps -> bpp == 16 || ps -> bpp == 15 || ps -> bpp == 8) {
		for(int y=block_y; y<block_y + block_h; y++) {
			const uint8_t *o = &img[y * i_W * 3 + block_x * 3];

			for(int x=block_x; x<block_x + block_w; x++) {
				int r = *o++;
				int g = *o++;
				int b = *o++;

				int dummy = 0;

				dummy = ((r >> (8 - ps -> red_bits)) << ps -> red_shift) |
					((g >> (8 - ps -> green_bits)) << ps -> green_shift) |
					((b >> (8 - ps -> blue_bits)) << ps -> blue_shift);

				if (ps -> bpp == 8)	// 8 bit
				{
					*p++ = dummy;
				}
				else if (ps -> big_endian) // 16/15 bit
				{
					*p++ = dummy >> 8;
					*p++ = dummy & 255;
				}
				else // 16/15 bit
				{
					*p++ = dummy & 255;
					*p++ = dummy >> 8;
				}
			}
		}
	}
}

bool send_block_raw(int fd, unsigned char *img, int i_W, int i_H, int block_x, int block_y, int block_w, int block_h, pixel_setup_t *ps)
{
	char *out = NULL;
	int len = 0;

	create_block_raw(fd, img, i_W, i_H, block_x, block_y, block_w, block_h, &out, &len, ps);

	char header[12 + 4] = { 0 };
	header[0] = 0;	// msg type
	header[1] = 0;	// padding
	put_card16(&header[2], 1);	// 1 rectangle
	put_card16(&header[4], block_x);	// x
	put_card16(&header[6], block_y);	// y
	put_card16(&header[8], block_w);	// w
	put_card16(&header[10], block_h);	// h
	put_card32(&header[12], 0);	// raw encoding

	if (WRITE(fd, header, sizeof header) == -1)
	{
		delete [] out;

		return false;
	}

	if (WRITE(fd, out, len) == -1)
	{
		delete [] out;

		return false;
	}

	delete [] out;

	return true;
}

bool send_full_screen(int fd, source *s, unsigned char *client_view, unsigned char *cur, int xpos, int ypos, int w, int h, pixel_setup_t *ps, enc_allowed_t *ea)
{
	bool rc = send_block_raw(fd, cur, w, h, xpos, ypos, w, h, ps);

	memcpy(client_view, cur, w * h * 3);

	return rc;
}

bool compare_do_block(const do_block_t b1, const do_block_t b2)
{
	// return b1.method < b2.method && b1.x < b2.x && b1.y < b2.y && b1.w < b2.w;
	return b1.method < b2.method || (b1.method == b2.method && (b1.x < b2.x || (b1.x == b2.x && (b1.y < b2.y || (b1.y == b2.y && b1.w < b2.w)))));
}

// check if solid color (std-dev luminance < 255 / (100 - fuzzy))
// dan: hextile, 1e byte: bit 2 set, dan bytes voor de kleur
bool solid_color(unsigned char *cur, int src_w, int src_h, int sx, int sy, int cur_bw, int cur_bh, double fuzzy, int *r, int *g, int *b)
{
	double rt = 0, gt = 0, bt = 0;
	double gray_t = 0, gray_sd = 0;

	for(int y=sy; y<sy + cur_bh; y++)
	{
		for(int x=sx; x<sx + cur_bw; x++)
		{
			int offset = y * src_w * 3 + x * 3;

			rt += cur[offset + 0];

			gt += cur[offset + 1];

			bt += cur[offset + 2];

			double lum = double(cur[offset + 0] + cur[offset + 1] + cur[offset + 2]) / 3.0;
			gray_t += lum;
			gray_sd += lum * lum; //pow(lum, 2.0);
		}
	}

	double n = cur_bw * cur_bh;

	double avg = gray_t / n;
	double sd = sqrt(gray_sd / n - pow(avg, 2.0));

	if (sd < 1.0)
	{
		*r = (int)(rt / n);
		*g = (int)(gt / n);
		*b = (int)(bt / n);

		return true;
	}

	return false;
}

bool send_incremental_screen(int fd, source *s, unsigned char *client_view, unsigned char *cur, int copy_x, int copy_y, int copy_w, int copy_h, double fuzzy, pixel_setup_t *ps, double *bw, enc_allowed_t *ea, int src_w, int src_h)
{
	std::map<uint32_t, block_t> cv_blocks; // client_view blocks

	constexpr int block_max_w = 16;
	constexpr int block_max_h = 16;

	// hash client_view
	for(int y=0; y<src_h; y += block_max_h)
	{
		int cur_bh = std::min(block_max_h, src_h - y);

		for(int x=0; x<src_w; x+= block_max_w)
		{
			int cur_bw = std::min(block_max_w, src_w - x);

			uint32_t val = hash_block(client_view, src_w, src_h, x, y, cur_bw, cur_bh, fuzzy);
			block_t b = { x, y, cur_bw, cur_bh };

			cv_blocks.insert(std::pair<uint32_t, block_t>(val, b));
		}
	}

	// see if any blocks in the new image match with the client_view
	std::vector<do_block_t> do_blocks; // blocks to send

	int xa = copy_x % block_max_w;
	int ya = copy_y % block_max_h;

	copy_x -= xa;
	copy_y -= ya;
	copy_w += xa;
	copy_h += ya;

	if (copy_w % block_max_w)
		copy_w = (copy_w / block_max_w) * block_max_w + block_max_w;

	if (copy_h % block_max_h)
		copy_h = (copy_h / block_max_h) * block_max_h + block_max_h;

	for(int y=copy_y; y<copy_y + copy_h; y += block_max_h)
	{
		int cur_bh = std::min(block_max_h, src_h - y);

		for(int x=copy_x; x<copy_x + copy_w; x+= block_max_w)
		{
			int cur_bw = std::min(block_max_w, src_w - x);

			uint32_t cur_val = hash_block(cur, src_w, src_h, x, y, cur_bw, cur_bh, fuzzy);

			auto loc = cv_blocks.find(cur_val);

			// did it change it all?
			do_block_t b = { ENC_NONE, x, y, cur_bw, cur_bh, loc -> second.x, loc -> second.y };

			bool put = false;
			if (loc == cv_blocks.end())	// block not found
			{
				if (ea -> hextile && solid_color(cur, src_w, src_h, x, y, cur_bw, cur_bh, fuzzy, &b.r, &b.g, &b.b))
					b.method = ENC_SOLID_COLOR;
				else
					b.method = ENC_RAW;

				put = true;
			}
			// block found on a different location
			else if (loc -> second.x != x && loc -> second.y != y && loc -> second.w == cur_bw && loc -> second.h == cur_bh)
			{
				b.method = ENC_COPY;
				put = true;
			}
			else
			{
				// block was found but at the current location so it did not change
			}

			// cluster rows of raw encoded blocks
			if (put == true)
			{
				do_blocks.push_back(b);

				if (b.method == ENC_SOLID_COLOR)
					fill_block(client_view, src_w, src_h, &b);
				else if (b.method == ENC_COPY || b.method == ENC_RAW)
					copy_block(client_view, cur, src_w, src_h, &b);
			}
		}
	}

	if (do_blocks.empty())
	{
		const int rbd = 8; // random block dimensions

		do_block_t b = { ENC_RAW, rand() % (src_w - rbd), rand() % (src_h - rbd), rbd, rbd, 0, 0 };

		do_blocks.push_back(b);
	}

	// merge
	int index = 0, merged = 0;
	while(index < int(do_blocks.size()) - 1)
	{
		do_block_t *p = &do_blocks.at(index);
		do_block_t *c = &do_blocks.at(index + 1);

		if (p -> method == ENC_RAW && c -> method == ENC_RAW && p -> x + p -> w == c -> x && p -> y == c -> y && p -> h == c -> h)
		{
			do_block_t n = { p -> method, p -> x, p -> y, p -> w + c -> w, p -> h, p -> src_x, p -> src_y };

			do_blocks.erase(do_blocks.begin() + index); // p
			do_blocks.erase(do_blocks.begin() + index); // c

			do_blocks.insert(do_blocks.begin() + index, n);

			merged++;
		}
		else
		{
			index++;
		}
	}

	int n_blocks = do_blocks.size();
	char msg_hdr[4] = { 0 };
	msg_hdr[0] = 0;	// msg type
	msg_hdr[1] = 0;	// padding
	put_card16(&msg_hdr[2], n_blocks);	// number of rectangles rectangle

	if (WRITE(fd, msg_hdr, sizeof msg_hdr) == -1)
		return false;

	// printf("%d,%d %dx%d / %d\n", copy_x, copy_y, copy_w, copy_h, merged);

	int solid_n = 0, copy_n = 0, raw_n = 0;
	int solid_np = 0, copy_np = 0, raw_np = 0;
	int bytes = 0;
	for(int index=0; index<n_blocks; index++)
	{
		do_block_t *b = &do_blocks.at(index);
		// printf("\t%d,%d %dx%d: %d\n", b -> x, b -> y, b -> w, b -> h, b -> method);

		if (b -> method == ENC_COPY)
		{
			copy_n++;

			char msg[12 + 4] = { 0 };

			put_card16(&msg[0], b -> x);	// x
			put_card16(&msg[2], b -> y);	// y
			put_card16(&msg[4], b -> w);	// w
			put_card16(&msg[6], b -> h);	// h
			put_card32(&msg[8], 1);	// copy rect
			put_card16(&msg[12], b -> src_x);	// x
			put_card16(&msg[14], b -> src_y);	// y

			if (WRITE(fd, msg, sizeof msg) == -1)
				return false;

			bytes += sizeof msg;

			copy_np += b -> w * b -> h;
		}
		else if (b -> method == ENC_RAW)
		{
			do_block_t *b = &do_blocks.at(index);

			raw_n++;
			raw_np += b -> w * b -> h;

			char msg[12] = { 0 };
			put_card16(&msg[0], b -> x);	// x
			put_card16(&msg[2], b -> y);	// y
			put_card16(&msg[4], b -> w);	// w
			put_card16(&msg[6], b -> h);	// h
			put_card32(&msg[8], 0);	// raw

			if (WRITE(fd, msg, sizeof msg) == -1)
				return false;

			bytes += sizeof msg;

			char *out = NULL;
			int len = 0;

			create_block_raw(fd, cur, src_w, src_h, b -> x, b -> y, b -> w, b -> h, &out, &len, ps);

			if (WRITE(fd, out, len) == -1)
			{
				delete [] out;
				return false;
			}

			bytes += len;

			delete [] out;
		}
		else if (b -> method == ENC_SOLID_COLOR)
		{
			solid_n++;

			char msg[12 + 1] = { 0 };
			put_card16(&msg[0], b -> x);	// x
			put_card16(&msg[2], b -> y);	// y
			put_card16(&msg[4], b -> w);	// w
			put_card16(&msg[6], b -> h);	// h
			put_card32(&msg[8], 5);	// hextile
			// bg color specified, AnySubrects is NOT set so whole rectangle is filled with bg color
			msg[12] = 2;

			if (WRITE(fd, msg, sizeof msg) == -1)
				return false;

			bytes += sizeof msg;

			int Bpp = 0;
			char buffer[4] = { 0 };
			encode_color(ps, b -> r, b -> g, b -> b, buffer, &Bpp);

			if (WRITE(fd, buffer, Bpp) == -1)
				return false;

			solid_np += b -> w * b -> h;

			bytes += sizeof Bpp;
		}
		else
		{
			error_exit(false, "Invalid (internal) enc mode %d", (int)b -> method);
		}
	}

	//int n_pixels = copy_np + solid_np + raw_np;

	//*bw = double(n_pixels * 100) / double(copy_w * copy_h);
	*bw = double(bytes * 100) / double(copy_w * copy_h * ps -> bpp / 8.0);

	printf("fuzz: %f, bytes: %.2f%%, copy: %d (%d), solid: %d (%d), raw: %d (%d)\n", fuzzy, *bw, copy_n, copy_np, solid_n, solid_np, raw_n, raw_np);

	return true;
}

bool send_screen(int fd, source *s, bool incremental, int xpos, int ypos, int w, int h, unsigned char *client_view, unsigned char *work, pixel_setup_t *ps, double *bw, double fuzzy, enc_allowed_t *ea, uint64_t *prev_ts)
{
	uint8_t *work_temp = NULL;
	size_t work_len = 0;

	for(;;) {
		int wt, ht;
		if (s -> get_frame(E_RGB, -1, prev_ts, &wt, &ht, &work_temp, &work_len))
			break;
	}

	memcpy(work, work_temp, work_len);
	free(work_temp);

	if (!incremental)
	{
		//*bw = 100;

		if (send_full_screen(fd, s, client_view, work, xpos, ypos, w, h, ps, ea) == false)
			return false;
	}
	else
	{
		if (send_incremental_screen(fd, s, client_view, work, xpos, ypos, w, h, fuzzy, ps, bw, ea, w, h) == false)
			return false;
	}

	return true;
}

bool SetColourMapEntries(int fd)
{
	unsigned char buffer[6 + 256 * 2 * 3] = { 0 };

	buffer[0] = 1;	// message type
	buffer[1] = 0;	// padding
	put_card16((char *)&buffer[2], 0);	// first color
	put_card16((char *)&buffer[4], 256);	// # colors;

	unsigned char *p = &buffer[6];
	for(int index=0; index<256; index++)
	{
		*p = index;
		p++;
		*p = index;
		p++;
		*p = index;
		p++;
		*p = index;
		p++;
		*p = index;
		p++;
		*p = index;
		p++;
	}

	return WRITE(fd, (char *)buffer, sizeof buffer) == sizeof buffer;
}

int count_bits(int value)
{
	int n = 0;

	while(value)
	{
		n++;

		value >>= 1;
	}

	return n;
}

void * vnc_main_loop(void *p)
{
	vnc_thread_t *vt = (vnc_thread_t *)p;

	vt -> s -> register_user();

	int fd = vt -> fd;
	source *s = vt -> s;

	pixel_setup_t ps;
	ps.red_max = ps.green_max = ps.blue_max = 255;
	ps.red_bits = ps.green_bits = ps.blue_bits = 8;
	ps.red_shift = 16;
	ps.green_shift = 8;
	ps.blue_shift = 0;
	ps.big_endian = false;
	ps.true_color = true;
	ps.bpp = 32;
	ps.depth = 8;

	int w = 0, h = 0;

	if (!handshake(fd, s, &ps, &w, &h))
		return NULL;

	bool big_endian = true;
	int bpp = 32;

	int bytes = w * h * 3;

	unsigned char *work = new unsigned char[bytes];

	unsigned char *client_view = new unsigned char[bytes];
	memset(client_view, 0x00, bytes);

	bool abort = false;

	enc_allowed_t ea = { true, true, true };

	double fuzzy = 1;
	double pts = get_ts(), pfull = 0;

	uint64_t ts = get_us();

	for(;!abort && !*vt -> local_stop_flag;) {
		char cmd[1] = { (char)-1 };

		if (READ(fd, cmd, 1) != 1)
			break;

		if (cmd[0] != 3)
			printf("cmd: %d\n", cmd[0]);

		switch(cmd[0]) {
			case 0:		// set pixel format [ignored]
			{
				char spf[20] = { 0 };
				if (READ(fd, &spf[1], sizeof spf - 1) != sizeof spf - 1)
				{
					abort = true;
					break;
				}

				ps.bpp = bpp = spf[4];
				printf("set bpp %d\n", bpp);
				printf("depth: %d\n", spf[5]);
				ps.big_endian = big_endian = spf[6];
				printf("big endian %d\n", big_endian);
				ps.true_color = spf[7];
				ps.red_max = get_card16(&spf[8]);
				printf("red max: %d\n", ps.red_max);
				ps.green_max = get_card16(&spf[10]);
				printf("green max: %d\n", ps.green_max);
				ps.blue_max = get_card16(&spf[12]);
				printf("blue max: %d\n", ps.blue_max);
				ps.red_shift = spf[14];
				printf("red shift: %d\n", ps.red_shift);
				ps.green_shift = spf[15];
				printf("green shift: %d\n", ps.green_shift);
				ps.blue_shift = spf[16];
				printf("blue shift: %d\n", ps.blue_shift);
				ps.red_bits = count_bits(ps.red_max);
				printf("red bits: %d\n", ps.red_bits);
				ps.green_bits = count_bits(ps.green_max);
				printf("green bits: %d\n", ps.green_bits);
				ps.blue_bits = count_bits(ps.blue_max);
				printf("blue bits: %d\n", ps.blue_bits);

				if (spf[7] == 0)
				{
					if (SetColourMapEntries(fd) == false)
					{
						abort = true;
						break;
					}
				}

				break;
			}

			case 2:		// encoding to use [ignored]
			{
				char ignore[1] = { 0 };
				if (READ(fd, ignore, sizeof ignore) != sizeof ignore)
				{
					abort = true;
					break;
				}

				int n_enc = read_card16(fd);
				if (n_enc < 0)
				{
					abort = true;
					break;
				}

				ea.raw = ea.copy = ea.hextile = false;
				bool enc_supported = false;
				for(int loop=0; loop<n_enc; loop++)
				{
					int32_t enc = read_card32(fd);
					//printf("enc: %d\n", enc);

					if (enc == 0)
						enc_supported = ea.raw = true;
					else if (enc == 1)
						enc_supported = ea.copy = true;
					else if (enc == 5)
						enc_supported = ea.hextile = true;
				}
				if (!enc_supported)
					printf("No supported encoding methods\n");

				break;
			}

			case 3:		// framebuffer update request
			{
				char incremental[1] = { 0 };
				if (READ(fd, incremental, sizeof incremental) != sizeof incremental)
				{
					abort = true;
					break;
				}

				int xpos = read_card16(fd);
				int ypos = read_card16(fd);
				int w = read_card16(fd);
				int h = read_card16(fd);

				// printf("%d,%d %dx%d: %s\n", xpos, ypos, w, h, incremental[0] ? "incremental" : "full");

				double cfullts = get_ts();

				if (incremental[0] && cfullts - pfull > 1.0 / 3.0)
				{
					incremental[0] = 0;
					pfull = cfullts;
				}

				double bw = 0.0;
				if (send_screen(fd, s, incremental[0], xpos, ypos, w, h, client_view, work, &ps, &bw, fuzzy, &ea, &ts) == false)
				{
					abort = true;
					break;
				}

				double ts = get_ts();
				double diff = ts - pts;
				if (diff == 0)
					diff = 0.01;	// FIXME fps
				pts = ts;

				double fps = 1.0 / diff;

				if (bw > 55)
				{
					fuzzy = (fuzzy * (fps + 1.0) + log(bw) * 10.0) / (fps + 1.0);

					if (fuzzy > 100)
						fuzzy = 100;
				}
				else if (bw < 45)
				{
					fuzzy = (fuzzy * (fps + 1.0) - log(100 - bw) * 10.0) / (fps + 1.0);

					if (fuzzy < 0)
						fuzzy = 0;
				}

				break;
			}

			case 4:	// key event
			{
				char ignore[7];
				if (READ(fd, ignore, sizeof ignore) != sizeof ignore)
				{
					abort = true;
					break;
				}

				// dolog("%c", ignore[6]);
				// fflush(NULL);

				break;
			}

			case 5:	// pointer event
			{
				char ignore[5];
				if (READ(fd, ignore, sizeof ignore) != sizeof ignore)
				{
					abort = true;
					break;
				}

				break;
			}

			case 6:	// copy paste
			{
				char ignore[3];
				if (READ(fd, ignore, sizeof ignore) != sizeof ignore)
				{
					abort = true;
					break;
				}

				int n = read_card32(fd);
				if (n < 0)
				{
					abort = true;
					break;
				}

				char *buffer = new char[n + 1];

				if (READ(fd, buffer, n) != n)
				{
					delete [] buffer;
					abort = true;
					break;
				}

				// buffer[n] = 0x00;
				// dolog("%s", buffer);
				// fflush(NULL);

				delete [] buffer;

				break;
			}

			default:
				// dolog("Don't know how to handle %d\n", cmd[0]);
				break;
		}
	}

	close(fd);

	delete [] client_view;
	delete [] work;

	vt -> s -> unregister_user();

	delete vt;

	return NULL;
}

target_vnc::target_vnc(const std::string & id, source *const s, const std::string & adapter, const int port, const int max_time, const double interval, const std::vector<filter *> *const filters, const std::string & exec_start, const std::string & exec_end) : target(id, s, "", "", max_time, interval, filters, exec_start, "", exec_end, -1) 
{
	fd = start_listen(adapter.c_str(), port, 5);
}

target_vnc::~target_vnc()
{
}

void target_vnc::operator()()
{
	set_thread_name("vnc_server");

	struct pollfd fds[] = { { fd, POLLIN, 0 } };

	std::vector<pthread_t> handles;

	for(;!local_stop_flag;) {
		for(size_t i=0; i<handles.size();) {
			if (pthread_tryjoin_np(handles.at(i), NULL) == 0)
				handles.erase(handles.begin() + i);
			else
				i++;
		}

		getMeta() -> setInt("vnc-viewers", std::pair<uint64_t, int>(get_us(), handles.size()));

		pauseCheck();

		if (poll(fds, 1, 500) == 0)
			continue;

		int cfd = accept(fd, NULL, 0);
		if (cfd == -1)
			continue;

		printf("VNC connected with: %s\n", get_endpoint_name(cfd).c_str());

		set_no_delay(cfd);

		vnc_thread_t *ct = new vnc_thread_t;

		ct -> fd = cfd;
		ct -> s = s;
		ct -> local_stop_flag = &local_stop_flag;

		pthread_t th;
		int rc = -1;
		if ((rc = pthread_create(&th, NULL, vnc_main_loop, ct)) != 0) {
			errno = rc;
			error_exit(true, "pthread_create failed (vnc client thread)");
		}

		handles.push_back(th);
	}

	for(size_t i=0; i<handles.size(); i++)
		pthread_join(handles.at(i), NULL);

	log(LL_INFO, "VNC server thread terminating");
}