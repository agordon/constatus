// (C) 2017-2018 by folkert van heusden, released under AGPL v3.0
#include <algorithm>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <netdb.h>
#include <png.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <jpeglib.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "config.h"
#ifdef WITH_EXIV2
#include <exiv2/exiv2.hpp>
#endif
#include <stdexcept>

#include "error.h"
#include "meta.h"
#include "log.h"

extern "C" {
#include <pbm.h>
}

static void libpng_error_handler(png_structp png, png_const_charp msg)
{
	error_exit(false, "libpng error: %s", msg);
}

static void libpng_warning_handler(png_structp png, png_const_charp msg)
{
	log(LL_WARNING, "libpng warning: %s", msg);
}

// rgbA (!)
void read_PNG_file_rgba(FILE *fh, int *w, int *h, uint8_t **pixels)
{
	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, libpng_error_handler, libpng_warning_handler);
	if (!png)
		error_exit(false, "png_create_read_struct(PNG_LIBPNG_VER_STRING) failed");

	png_infop info = png_create_info_struct(png);
	if (!info)
		error_exit(false, "png_create_info_struct failed");

	if (setjmp(png_jmpbuf(png))) {
		log(LL_INFO, "PNG decode error");
		return;
	}

	png_init_io(png, fh);

	png_read_info(png, info);

	*w = png_get_image_width(png, info);
	*h = png_get_image_height(png, info);
	int color_type = png_get_color_type(png, info);
	int bit_depth = png_get_bit_depth(png, info);

	// Read any color_type into 8bit depth, RGBA format.
	// See http://www.libpng.org/pub/png/libpng-manual.txt

	if (bit_depth == 16)
		png_set_strip_16(png);

	if(color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_palette_to_rgb(png);

	// PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth.
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_expand_gray_1_2_4_to_8(png);

	if (png_get_valid(png, info, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png);

	// These color_type don't have an alpha channel then fill it with 0xff.
	if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE)
		png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

	if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png);

	png_read_update_info(png, info);

	*pixels = (uint8_t *)valloc(*w * *h * 4);

	png_bytep *row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * *h);
	for(int y = 0; y < *h; y++)
		row_pointers[y] = &(*pixels)[y * *w * 4];

	png_read_image(png, row_pointers);
	free(row_pointers);

	png_destroy_read_struct(&png, &info, nullptr);
}

void write_PNG_file(FILE *fh, int ncols, int nrows, unsigned char *pixels)
{
	png_bytep *row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * nrows);
	if (!row_pointers)
		error_exit(true, "write_PNG_file error allocating row-pointers");
	for(int y=0; y<nrows; y++)
		row_pointers[y] = &pixels[y*ncols*3];

	png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, libpng_error_handler, libpng_warning_handler);
	if (!png)
		error_exit(false, "png_create_write_struct failed");

	png_infop info = png_create_info_struct(png);
	if (info == NULL)
		error_exit(false, "png_create_info_struct failed");

	png_init_io(png, fh);

	png_set_compression_level(png, 3);

	png_set_IHDR(png, info, ncols, nrows, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	png_text text_ptr[2];
	text_ptr[0].key = (png_charp)"Author";
	text_ptr[0].text = (png_charp)NAME " " VERSION;
	text_ptr[0].compression = PNG_TEXT_COMPRESSION_NONE;
	text_ptr[1].key = (png_charp)"URL";
	text_ptr[1].text = (png_charp)"http://www.vanheusden.com/constatus/";
	text_ptr[1].compression = PNG_TEXT_COMPRESSION_NONE;
	png_set_text(png, info, text_ptr, 2);

	png_write_info(png, info);

	png_write_image(png, row_pointers);

	png_write_end(png, NULL);

	png_destroy_write_struct(&png, &info);

	free(row_pointers);
}

void write_JPEG_memory(meta *const m, const int ncols, const int nrows, const int quality, const unsigned char *const pixels, char **out, size_t *out_len)
{
	uint8_t *temp = NULL;
	size_t temp_len = 0;

	//// GENERATE JPEG ////

	FILE *fh = open_memstream((char **)&temp, &temp_len);

	struct jpeg_compress_struct cinfo;
	memset(&cinfo, 0x00, sizeof cinfo);

	struct jpeg_error_mgr jerr;
	cinfo.err = jpeg_std_error(&jerr);

	jpeg_create_compress(&cinfo);

	jpeg_stdio_dest(&cinfo, fh);

	cinfo.image_width = ncols;
	cinfo.image_height = nrows;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE);

	jpeg_start_compress(&cinfo, TRUE);

	const char comment[] = NAME " " VERSION;
	jpeg_write_marker(&cinfo, JPEG_COM, (const JOCTET *)comment, sizeof comment);

	int row_stride = ncols * 3;
	while(cinfo.next_scanline < cinfo.image_height)
	{
		JSAMPROW row_pointer[1] = { (unsigned char *)&pixels[cinfo.next_scanline * row_stride] };

		(void)jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);

	fclose(fh);

	//// add EXIF ////

	std::pair<uint64_t, double> longitude, latitude;

#ifdef WITH_EXIV2
	if (m && m -> get_double("$longitude$", &longitude) && m -> get_double("$latitude$", &latitude)) {
		Exiv2::Image::AutoPtr image = Exiv2::ImageFactory::open((const Exiv2::byte *)temp, temp_len);
		Exiv2::ExifData exifData;

		if (exifData.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSVersionID")) == exifData.end()) {
			Exiv2::Value::AutoPtr value = Exiv2::Value::create(Exiv2::unsignedByte);
			value->read("2 0 0 0");
			exifData.add(Exiv2::ExifKey("Exif.GPSInfo.GPSVersionID"), value.get());

			exifData["Exif.GPSInfo.GPSMapDatum"] = "WGS-84";
		}

		exifData["Exif.GPSInfo.GPSLatitudeRef"] = (latitude.second < 0 ) ? "S" : "N";

		int degLatitude = fabs(latitude.second);
		int minLatitude = (fabs(latitude.second) - degLatitude)*60;
		int secLatitude = ((fabs(latitude.second) - degLatitude)*60 - minLatitude)*60;

		char scratchBufLatitude[128];
		snprintf(scratchBufLatitude, sizeof scratchBufLatitude, "%d/1 %d/1 %d/1", degLatitude, minLatitude, secLatitude);
		exifData["Exif.GPSInfo.GPSLatitude"] = scratchBufLatitude;

		exifData["Exif.GPSInfo.GPSLongitudeRef"] = (longitude.second < 0 ) ? "W" : "E";

		int degLongitude = fabs(longitude.second);
		int minLongitude = (fabs(longitude.second) - degLongitude)*60;
		int secLongitude = ((fabs(longitude.second) - degLongitude)*60 - minLongitude )*60;

		char scratchBufLongitude[128];
		snprintf(scratchBufLongitude, sizeof scratchBufLongitude, "%d/1 %d/1 %d/1", degLongitude, minLongitude, secLongitude);
		exifData["Exif.GPSInfo.GPSLongitude"] = scratchBufLongitude;

		image->setExifData(exifData);
		image->writeMetadata();

		*out_len = image->io().size();

		*out = (char *)malloc(*out_len);
		memcpy(*out, image->io().mmap(), *out_len);

		free(temp);
	}
	else
#endif
	{
		*out = (char *)temp;
		*out_len = temp_len;
	}
}

void write_JPEG_file(meta *const m, FILE *const fh, const int ncols, const int nrows, const int quality, const unsigned char *pixels)
{
	char *out = NULL;
	size_t out_len = 0;
	write_JPEG_memory(m, ncols, nrows, quality, pixels, &out, &out_len);

	fwrite(out, out_len, 1, fh);

	free(out);
}

void jpegErrorExit(j_common_ptr cinfo)
{
	char err[JMSG_LENGTH_MAX];
	(*(cinfo->err->format_message))(cinfo, err);
	//log("JPEG error: %s", err);

	throw std::runtime_error(err);
}

void jpegOutputMessage(j_common_ptr cinfo)
{
	char buffer[JMSG_LENGTH_MAX];
	(*cinfo->err->format_message)(cinfo, buffer);

	log(LL_WARNING, "JPEG: %s", buffer);
}

bool read_JPEG_memory(unsigned char *in, int n_bytes_in, int *w, int *h, unsigned char **pixels)
{
	bool ok = true;
	FILE *fh = fmemopen(in, n_bytes_in, "rb");

	struct jpeg_decompress_struct info;

	struct jpeg_error_mgr err;
	info.err = jpeg_std_error(&err);
	info.err -> error_exit = jpegErrorExit;
	info.err -> output_message = jpegOutputMessage;

	try {
		jpeg_create_decompress(&info);

		jpeg_stdio_src(&info, fh);
		jpeg_read_header(&info, true);

		jpeg_start_decompress(&info);

		*pixels = NULL;
		*w = info.output_width;
		*h = info.output_height;

		if (info.num_components != 3) {
			log(LL_FATAL, "JPEG: unexpected number of color components (%u) - RGB is required", info.num_components);
			return false;
		}

		unsigned long dataSize = *w * *h * info.num_components;

		*pixels = (unsigned char *)valloc(dataSize);
		while(info.output_scanline < *h) {
			unsigned char *rowptr = *pixels + info.output_scanline * *w * info.num_components;

			jpeg_read_scanlines(&info, &rowptr, 1);
		}

		jpeg_finish_decompress(&info);    
	}
	catch(std::runtime_error & e) {
		log(LL_ERR, "JPEG runtime error: %s", e.what());
		free(*pixels);
		*pixels = NULL;
		ok = false;
	}

	fclose(fh);

	jpeg_destroy_decompress(&info);

	return ok;
}

void load_PBM_file(FILE *const fh, int *const w, int *const h, uint8_t **out)
{
	bit **pbm = pbm_readpbm(fh, w, h);

	*out = (uint8_t *)valloc(*w * *h * sizeof(uint8_t));

	for(int y=0; y<*h; y++) {
		for(int x=0; x<*w; x++)
			(*out)[y * *w + x] = pbm[y][x];
	}

	pbm_freearray(pbm, *h);
}
