// (C) 2017-2018 by folkert van heusden, released under AGPL v3.0
#include <algorithm>
#include <atomic>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <jansson.h>
#include <netdb.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
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

#include "error.h"
#include "http_server.h"
#include "picio.h"
#include "source.h"
#include "utils.h"
#include "log.h"
#include "motion_trigger.h"
#include "target.h"
#include "target_avi.h"
#include "target_ffmpeg.h"

typedef struct {
	instance_t *inst;
	int fd;
	double fps;
	int quality;
	int time_limit;
	const std::vector<filter *> *filters;
	std::atomic_bool *global_stopflag;
	resize *r;
	int resize_w, resize_h;
	bool motion_compatible, allow_admin, archive_acces, is_rest;
	configuration_t *cfg;
	std::string snapshot_dir;
	std::vector<view *> *views;
} http_thread_t;

void send_mjpeg_view_stream(const int cfd, view *v, const double fps, const int quality, const bool get, const int time_limit, std::atomic_bool *const global_stopflag)
{
        const char reply_headers[] =
                "HTTP/1.0 200 OK\r\n"
                "Cache-Control: no-cache\r\n"
                "Pragma: no-cache\r\n"
		"Server: " NAME " " VERSION "\r\n"
                "Expires: thu, 01 dec 1994 16:00:00 gmt\r\n"
                "Connection: close\r\n"
                "Content-Type: multipart/x-mixed-replace; boundary=--myboundary\r\n"
                "\r\n";

	if (WRITE(cfd, reply_headers, strlen(reply_headers)) <= 0)
	{
		log(LL_DEBUG, "short write on response header");
		close(cfd);
		return;
	}

	if (!get)
	{
		close(cfd);
		return;
	}

	bool first = true;

	uint64_t prev = 0;
	time_t end = time(NULL) + time_limit;
	for(;(time_limit <= 0 || time(NULL) < end) && !*global_stopflag;)
	{
		int w = -1, h = -1;
		uint8_t *work = NULL;
		size_t work_len = 0;
		v -> get_frame(E_JPEG, quality, &prev, &w, &h, &work, &work_len);

		// send header
                const char term[] = "\r\n";
                if (first)
                        first = false;
                else if (WRITE(cfd, term, strlen(term)) <= 0)
		{
			log(LL_DEBUG, "short write on terminating cr/lf");
			free(work);
                        break;
		}

		char img_h[4096] = { 0 };
		int len = snprintf(img_h, sizeof img_h, 
			"--myboundary\r\n"
			"Content-Type: image/jpeg\r\n"
			"Content-Length: %zu\r\n"
			"\r\n", work_len);
		if (WRITE(cfd, img_h, len) <= 0)
		{
			log(LL_DEBUG, "short write on boundary header");
			free(work);
			break;
		}

		if (WRITE(cfd, reinterpret_cast<char *>(work), work_len) <= 0)
		{
			log(LL_DEBUG, "short write on img data");
			free(work);
			break;
		}

		free(work);

		// FIXME adapt this by how long the wait_for_frame took
		if (fps > 0) {
			double us = 1000000.0 / fps;
			if (us)
				usleep((useconds_t)us);
		}
	}
}

void send_mjpeg_stream(int cfd, source *s, double fps, int quality, bool get, int time_limit, const std::vector<filter *> *const filters, std::atomic_bool *const global_stopflag, resize *const r, const int resize_w, const int resize_h)
{
        const char reply_headers[] =
                "HTTP/1.0 200 OK\r\n"
                "Cache-Control: no-cache\r\n"
                "Pragma: no-cache\r\n"
		"Server: " NAME " " VERSION "\r\n"
                "Expires: thu, 01 dec 1994 16:00:00 gmt\r\n"
                "Connection: close\r\n"
                "Content-Type: multipart/x-mixed-replace; boundary=--myboundary\r\n"
                "\r\n";

	if (WRITE(cfd, reply_headers, strlen(reply_headers)) <= 0)
	{
		log(LL_DEBUG, "short write on response header");
		close(cfd);
		return;
	}

	if (!get)
	{
		close(cfd);
		return;
	}

	bool first = true;
	uint8_t *prev_frame = NULL;
	bool sc = resize_h != -1 || resize_w != -1;

	uint64_t prev = 0;
	time_t end = time(NULL) + time_limit;
	for(;(time_limit <= 0 || time(NULL) < end) && !*global_stopflag;)
	{
		int w = -1, h = -1;
		uint8_t *work = NULL;
		size_t work_len = 0;
		s -> get_frame(filters -> empty() && !sc ? E_JPEG : E_RGB, quality, &prev, &w, &h, &work, &work_len);

		// send header
                const char term[] = "\r\n";
                if (first)
                        first = false;
                else if (WRITE(cfd, term, strlen(term)) <= 0)
		{
			log(LL_DEBUG, "short write on terminating cr/lf");
			free(work);
                        break;
		}

		// decode, encode, etc frame
		if (filters -> empty() && !sc) {
			char img_h[4096] = { 0 };
			int len = snprintf(img_h, sizeof img_h, 
				"--myboundary\r\n"
				"Content-Type: image/jpeg\r\n"
				"Content-Length: %zu\r\n"
				"\r\n", work_len);
			if (WRITE(cfd, img_h, len) <= 0)
			{
				log(LL_DEBUG, "short write on boundary header");
				free(work);
				break;
			}

			if (WRITE(cfd, reinterpret_cast<char *>(work), work_len) <= 0)
			{
				log(LL_DEBUG, "short write on img data");
				free(work);
				break;
			}

			free(work);
		}
		else
		{
			apply_filters(filters, prev_frame, work, prev, w, h);

			char *data_out = NULL;
			size_t data_out_len = 0;
			FILE *fh = open_memstream(&data_out, &data_out_len);
			if (!fh)
				error_exit(true, "open_memstream() failed");

			if (resize_h != -1 || resize_w != -1) {
				int target_w = resize_w != -1 ? resize_w : w;
				int target_h = resize_h != -1 ? resize_h : h;

				uint8_t *temp = NULL;
				r -> do_resize(w, h, work, target_w, target_h, &temp);

				write_JPEG_file(s -> get_meta(), fh, target_w, target_h, quality, temp);
				free(temp);
			}
			else {
				write_JPEG_file(s -> get_meta(), fh, w, h, quality, work);
			}

			fclose(fh);

			free(prev_frame);
			prev_frame = work;

			char img_h[4096] = { 0 };
			int len = snprintf(img_h, sizeof img_h, 
				"--myboundary\r\n"
				"Content-Type: image/jpeg\r\n"
				"Content-Length: %zu\r\n"
				"\r\n", data_out_len);
			if (WRITE(cfd, img_h, len) <= 0)
			{
				log(LL_DEBUG, "short write on boundary header");
				break;
			}

			if (WRITE(cfd, data_out, data_out_len) <= 0)
			{
				log(LL_DEBUG, "short write on img data");
				break;
			}

			free(data_out);
			data_out = NULL;
		}

		// FIXME adapt this by how long the wait_for_frame took
		if (fps > 0) {
			double us = 1000000.0 / fps;
			if (us)
				usleep((useconds_t)us);
		}
	}

	free(prev_frame);
}

void send_mpng_stream(int cfd, source *s, double fps, bool get, const int time_limit, const std::vector<filter *> *const filters, std::atomic_bool *const global_stopflag, resize *const r, const int resize_w, const int resize_h)
{
        const char reply_headers[] =
                "HTTP/1.0 200 ok\r\n"
                "cache-control: no-cache\r\n"
                "pragma: no-cache\r\n"
		"server: " NAME " " VERSION "\r\n"
                "expires: thu, 01 dec 1994 16:00:00 gmt\r\n"
                "connection: close\r\n"
                "content-type: multipart/x-mixed-replace; boundary=--myboundary\r\n"
                "\r\n";

	if (WRITE(cfd, reply_headers, strlen(reply_headers)) <= 0)
	{
		log(LL_DEBUG, "short write on response header");
		close(cfd);
		return;
	}

	if (!get)
	{
		close(cfd);
		return;
	}

	bool first = true;
	char *data_out = NULL;
	size_t data_out_len = 0;

	uint8_t *prev_frame = NULL;

	uint64_t prev = 0;
	time_t end = time(NULL) + time_limit;
	for(;(time_limit <= 0 || time(NULL) < end) && !*global_stopflag;)
	{
		int w = -1, h = -1;
		uint8_t *work = NULL;
		size_t work_len = 0;
		s -> get_frame(E_RGB, -1, &prev, &w, &h, &work, &work_len);

		apply_filters(filters, prev_frame, work, prev, w, h);

		data_out = NULL;
		data_out_len = 0;
		FILE *fh = open_memstream(&data_out, &data_out_len);
		if (!fh)
			error_exit(true, "open_memstream() failed");

		if (resize_w != -1 || resize_h != -1) {
			int target_w = resize_w != -1 ? resize_w : w;
			int target_h = resize_h != -1 ? resize_h : h;

			uint8_t *temp = NULL;
			r -> do_resize(w, h, work, target_w, target_h, &temp);

			write_PNG_file(fh, target_w, target_h, temp);

			free(temp);
		}
		else {
			write_PNG_file(fh, w, h, work);
		}

		fclose(fh);

		free(prev_frame);
		prev_frame = work;

		// send
                const char term[] = "\r\n";
                if (first)
                        first = false;
                else if (WRITE(cfd, term, strlen(term)) <= 0)
		{
			log(LL_DEBUG, "short write on terminating cr/lf");
                        break;
		}

                char img_h[4096] = { 0 };
		snprintf(img_h, sizeof img_h, 
                        "--myboundary\r\n"
                        "Content-Type: image/png\r\n"
			"Content-Length: %d\r\n"
                        "\r\n", (int)data_out_len);
		if (WRITE(cfd, img_h, strlen(img_h)) <= 0)
		{
			log(LL_DEBUG, "short write on boundary header");
			break;
		}

		if (WRITE(cfd, data_out, data_out_len) <= 0)
		{
			log(LL_DEBUG, "short write on img data");
			break;
		}

		free(data_out);
		data_out = NULL;

		// FIXME adapt this by how long the wait_for_frame took
		double us = 1000000.0 / fps;

		if (us)
			usleep((useconds_t)us);
	}

	free(data_out);

	free(prev_frame);
}

void send_png_frame(int cfd, source *s, bool get, const std::vector<filter *> *const filters, resize *const r, const int resize_w, const int resize_h)
{
        const char reply_headers[] =
                "HTTP/1.0 200 ok\r\n"
                "cache-control: no-cache\r\n"
                "pragma: no-cache\r\n"
		"server: " NAME " " VERSION "\r\n"
                "expires: thu, 01 dec 1994 16:00:00 gmt\r\n"
                "connection: close\r\n"
                "content-type: image/png\r\n"
                "\r\n";

	if (WRITE(cfd, reply_headers, strlen(reply_headers)) <= 0) {
		log(LL_DEBUG, "short write on response header");
		close(cfd);
		return;
	}

	if (!get) {
		close(cfd);
		return;
	}

	set_no_delay(cfd, true);

	uint64_t prev_ts = get_us();
	int w = -1, h = -1;
	uint8_t *work = NULL;
	size_t work_len = 0;
	s -> get_frame(E_RGB, -1, &prev_ts, &w, &h, &work, &work_len);

	char *data_out = NULL;
	size_t data_out_len = 0;

	FILE *fh = open_memstream(&data_out, &data_out_len);
	if (!fh)
		error_exit(true, "open_memstream() failed");

	apply_filters(filters, NULL, work, prev_ts, w, h);

	if (resize_h != -1 || resize_w != -1) {
		int target_w = resize_w != -1 ? resize_w : w;
		int target_h = resize_h != -1 ? resize_h : h;

		uint8_t *temp = NULL;
		r -> do_resize(w, h, work, target_w, target_h, &temp);

		write_PNG_file(fh, target_w, target_h, temp);

		free(temp);
	}
	else {
		write_PNG_file(fh, w, h, work);
	}

	free(work);

	fclose(fh);

	if (WRITE(cfd, data_out, data_out_len) <= 0)
		log(LL_DEBUG, "short write on img data");

	free(data_out);
}

void send_jpg_frame(int cfd, source *s, bool get, int quality, const std::vector<filter *> *const filters, resize *const r, const int resize_w, const int resize_h)
{
        const char reply_headers[] =
                "HTTP/1.0 200 ok\r\n"
                "cache-control: no-cache\r\n"
                "pragma: no-cache\r\n"
		"server: " NAME " " VERSION "\r\n"
                "expires: thu, 01 dec 1994 16:00:00 gmt\r\n"
                "connection: close\r\n"
                "content-type: image/jpeg\r\n"
                "\r\n";

	if (WRITE(cfd, reply_headers, strlen(reply_headers)) <= 0) {
		log(LL_DEBUG, "short write on response header");
		close(cfd);
		return;
	}

	if (!get)
	{
		close(cfd);
		return;
	}

	set_no_delay(cfd, true);

	bool sc = resize_h != -1 || resize_w != -1;

	uint64_t prev_ts = get_us();
	int w = -1, h = -1;
	uint8_t *work = NULL;
	size_t work_len = 0;
	s -> get_frame(filters -> empty() && !sc ? E_JPEG : E_RGB, quality, &prev_ts, &w, &h, &work, &work_len);

	char *data_out = NULL;
	size_t data_out_len = 0;
	FILE *fh = open_memstream(&data_out, &data_out_len);
	if (!fh)
		error_exit(true, "open_memstream() failed");

	if (filters -> empty() && !sc)
		fwrite(work, work_len, 1, fh);
	else {
		apply_filters(filters, NULL, work, prev_ts, w, h);

		if (resize_h != -1 || resize_w != -1) {
			int target_w = resize_w != -1 ? resize_w : w;
			int target_h = resize_h != -1 ? resize_h : h;

			uint8_t *temp = NULL;
			r -> do_resize(w, h, work, target_w, target_h, &temp);

			write_JPEG_file(s -> get_meta(), fh, target_w, target_h, quality, temp);
			free(temp);
		}
		else {
			write_JPEG_file(s -> get_meta(), fh, w, h, quality, work);
		}
	}

	fclose(fh);

	if (WRITE(cfd, data_out, data_out_len) <= 0)
		log(LL_DEBUG, "short write on img data");

	free(data_out);

	free(work);
}

std::string describe_interface(const instance_t *const inst, const interface *const i)
{
	std::string inst_url = url_escape(inst -> name);

	std::string module_int = i -> get_id();

	std::string type = "!!!", div = "some_section";
	switch(i -> get_class_type()) {
		case CT_HTTPSERVER:
			type = "HTTP server";
			div = "http-server";
			break;
		case CT_MOTIONTRIGGER:
			type = "motion trigger";
			div = "motion-trigger";
			break;
		case CT_TARGET:
			type = "stream writer";
			div = "stream-writer";
			break;
		case CT_SOURCE:
			type = "source";
			div = "source";
			break;
		case CT_LOOPBACK:
			type = "video4linux loopback";
			div = "v4l-loopback";
			break;
		case CT_NONE:
			type = "???";
			break;
	}

	std::string out = std::string("<div id=\"") + div + "\"><h2>" + type + "</h2><p>description: " + i -> get_descr() + "<br>type: " + type + "</p><ul>";

	if (i -> is_paused()) {
		out += myformat("<li><a href=\"unpause?inst=%s&module=%s\">unpause</a>", inst_url.c_str(), module_int.c_str());
	}
	else {
		out += myformat("<li><a href=\"pause?inst=%s&module=%s\">pause</a>", inst_url.c_str(), module_int.c_str());
	}

	if (i -> is_running()) {
		out += myformat("<li><a href=\"stop?inst=%s&module=%s\">stop</a>", inst_url.c_str(), module_int.c_str());
	}
	else {
		out += myformat("<li><a href=\"start?inst=%s&module=%s\">start</a>", inst_url.c_str(), module_int.c_str());
	}
	out += myformat("<li><a href=\"restart?inst=%s&module=%s\">restart</a>", inst_url.c_str(), module_int.c_str());

	out += "</ul></div>";

	return out;
}

bool pause(instance_t *const cfg, const std::string & which, const bool p)
{
	bool rc = false;

	interface *i = find_by_id(cfg, which);
	if (i != NULL) {
		if (p)
			rc = i -> pause();
		else {
			i -> unpause();
			rc = true;
		}
	}

	return rc;
}

bool start_stop(interface *const i, const bool strt)
{
	bool rc = false;

	if (i != NULL) {
		if (strt)
			i -> start();
		else
			i -> stop(); // FIXME time-out?
		rc = true;
	}

	return rc;
}

bool take_a_picture(source *const s, const std::string & snapshot_dir, const int quality)
{
	uint64_t prev_ts = get_us();
	int w = -1, h = -1;
	uint8_t *work = NULL;
	size_t work_len = 0;
	s -> get_frame(E_JPEG, quality, &prev_ts, &w, &h, &work, &work_len);

	std::string name = gen_filename(snapshot_dir, "snapshot-", "jpg", get_us(), 0);

	log(LL_INFO, "Snapshot will be written to %s", name.c_str());

	FILE *fh = fopen(name.c_str(), "w");
	if (!fh) {
		log(LL_ERR, "open_memstream() failed");
		free(work);
		return false;
	}

	bool rc = fwrite(work, 1, work_len, fh) == work_len;

	fclose(fh);

	free(work);

	return rc;
}

interface * start_a_video(source *const s, const std::string & snapshot_dir, const int quality)
{
	const std::string id = myformat("%ld", rand()); // FIXME better rand
	const std::string descr = "snapshot started by HTTP server";

	std::vector<filter *> *const filters = new std::vector<filter *>();

#ifdef WITH_GWAVI
	interface *i = new target_avi(id, descr, s, snapshot_dir, "snapshot-", quality, -1, -1, filters, "", "", "", -1);
#else
	interface *i = new target_ffmpeg(id, descr, "", s, snapshot_dir, "snapshot-", -1, -1, "mp4", 201000, filters, "", "", "", -1);
#endif
	i -> start();

	return i;
}

bool validate_file(const std::string & snapshot_dir, const std::string & filename)
{
	bool rc = false;

	auto *files = load_filelist(snapshot_dir, "");

	for(auto cur : *files) {
		if (cur.name == filename) {
			rc = true;
			break;
		}
	}

	delete files;

	return rc;
}

std::string run_rest(configuration_t *const cfg, const std::string & path, const std::string & pars, const std::string & snapshot_dir, const int quality)
{
	int code = 200;
	std::vector<std::string> *parts = split(path, "/");

	json_t *json = json_object();

	bool cmd = parts -> at(0) == "cmd";

	cfg -> lock.lock();
	interface *i = (parts -> size() >= 1 && !cmd) ? find_by_id(cfg, parts -> at(0)) : NULL;

	if (parts -> size() < 2) {
		code = 500;
		json_object_set_new(json, "msg", json_string("ID/cmd missing"));
		json_object_set_new(json, "result", json_false());
	}
	else if (cmd && parts -> at(1) == "list") {
		json_object_set_new(json, "msg", json_string("OK"));
		json_object_set_new(json, "result", json_true());

		json_t *out_arr = json_array();
		json_object_set_new(json, "data", out_arr);

		for(instance_t * inst : cfg -> instances) {
			for(interface *cur : inst -> interfaces) {
				json_t *record = json_object();

				json_object_set_new(record, "id", json_string(cur -> get_id().c_str()));

				json_object_set_new(record, "id-descr", json_string(cur -> get_descr().c_str()));

				switch(cur -> get_class_type()) {
					case CT_HTTPSERVER:
						json_object_set_new(record, "type", json_string("http-server"));
						break;
					case CT_MOTIONTRIGGER:
						json_object_set_new(record, "type", json_string("motion-trigger"));
						break;
					case CT_TARGET:
						json_object_set_new(record, "type", json_string("stream-writer"));
						break;
					case CT_SOURCE:
						json_object_set_new(record, "type", json_string("source"));
						break;
					case CT_LOOPBACK:
						json_object_set_new(record, "type", json_string("video4linux-loopback"));
						break;
					default:
						json_object_set_new(record, "type", json_string("INTERNAL ERROR"));
						break;
				}

				json_object_set_new(record, "running", json_string(cur -> is_paused() ? "false" : "true"));

				json_object_set_new(record, "enabled", json_string(cur -> is_running() ? "true" : "false"));

				json_array_append_new(out_arr, record);
			}
		}
	}
	// commands that require an ID
	else if (i == NULL) {
		code = 404;
		json_object_set_new(json, "msg", json_string("ID not found"));
		json_object_set_new(json, "result", json_false());
	}
	else if (cmd && parts -> at(1) == "list-files" && i -> get_class_type() == CT_TARGET) {
		json_object_set_new(json, "msg", json_string("OK"));
		json_object_set_new(json, "result", json_true());

		json_t *out_arr = json_array();
		json_object_set_new(json, "data", out_arr);

		auto *files = load_filelist(((target *)i) -> get_target_dir(), "");

		for(auto file : *files) {
			json_t *record = json_object();

			json_object_set_new(record, "file", json_string(file.name.c_str()));
			json_object_set_new(record, "mtime", json_integer(file.last_change));
			json_object_set_new(record, "size", json_integer(file.size));

			json_array_append_new(out_arr, record);
		}

		delete files;
	}
	else if (cmd && parts -> at(1) == "take-a-snapshot" && i -> get_class_type() == CT_SOURCE) {
		if (take_a_picture((source *)i, snapshot_dir, quality)) {
			json_object_set_new(json, "msg", json_string("OK"));
			json_object_set_new(json, "result", json_true());
		}
		else {
			json_object_set_new(json, "msg", json_string("failed"));
			json_object_set_new(json, "result", json_false());
		}
	}
	else if (cmd && parts -> at(1) == "start-a-recording" && i -> get_class_type() == CT_SOURCE) {
		interface *str = start_a_video((source *)i, snapshot_dir, quality);

		if (str) {
			json_object_set_new(json, "msg", json_string("OK"));
			json_object_set_new(json, "result", json_true());

			find_instance_by_interface(cfg, i) -> interfaces.push_back(str);
		}
		else {
			json_object_set_new(json, "msg", json_string("failed"));
			json_object_set_new(json, "result", json_false());
		}

	}
	else if (parts -> at(1) == "pause") {
		if (i -> pause()) {
			json_object_set_new(json, "msg", json_string("OK"));
			json_object_set_new(json, "result", json_true());
		}
		else {
			json_object_set_new(json, "msg", json_string("failed"));
			json_object_set_new(json, "result", json_false());
		}
	}
	else if (parts -> at(1) == "unpause") {
		i -> unpause();
		json_object_set_new(json, "msg", json_string("OK"));
		json_object_set_new(json, "result", json_true());
	}
	else if (parts -> at(1) == "stop" || parts -> at(1) == "start") {
		if (start_stop(i, parts -> at(1) == "start")) {
			json_object_set_new(json, "msg", json_string("OK"));
			json_object_set_new(json, "result", json_true());
		}
		else {
			json_object_set_new(json, "msg", json_string("failed"));
			json_object_set_new(json, "result", json_false());
		}
	}
	else if (parts -> at(1) == "configure-motion-trigger" && parts -> size() == 4) {
		json_decref(json);
		json = ((motion_trigger *)i) -> rest_set_parameter(parts -> at(2), parts -> at(3));
	}
	else if (parts -> at(1) == "list-motion-trigger") {
		json_t *result = ((motion_trigger *)i) -> get_rest_settable_parameters();

		json_object_set_new(json, "msg", json_string("OK"));
		json_object_set_new(json, "result", json_true());
		json_object_set_new(json, "data", result);
	}
	else {
		code = 404;
	}

	cfg -> lock.unlock();

	delete parts;

	char *js = json_dumps(json, JSON_COMPACT);
	std::string reply = myformat("HTTP/1.0 %d\r\nServer: " NAME " " VERSION "\r\nContent-Type: application/json\r\n\r\n%s", code, js);
	free(js);

	json_decref(json);

	return reply;
}

void handle_rest(int cfd, configuration_t *const cfg, std::atomic_bool *const global_stopflag, const std::string & snapshot_dir, const int quality)
{
	sigset_t all_sigs;
	sigfillset(&all_sigs);
	pthread_sigmask(SIG_BLOCK, &all_sigs, NULL);

	char request_headers[65536] = { 0 };
	int h_n = 0;

	struct pollfd fds[1] = { { cfd, POLLIN, 0 } };

	for(;!*global_stopflag;) {
		if (poll(fds, 1, 250) == 0)
			continue;

		int rc = read(cfd, &request_headers[h_n], sizeof request_headers - h_n - 1);
		if (rc == -1)
		{
			if (errno == EINTR || errno == EAGAIN)
				continue;

			log(LL_DEBUG, "error receiving request headers");
			close(cfd);
			return;
		}
		else if (rc == 0)
		{
			log(LL_DEBUG, "error receiving request headers");
			close(cfd);
			return;
		}

		h_n += rc;

		request_headers[h_n] = 0x00;

		if (strstr(request_headers, "\r\n\r\n") || strstr(request_headers, "\n\n"))
			break;
	}

	char *Path = NULL;

	bool get = true;
	if (strncmp(request_headers, "GET ", 4) == 0)
	{
		get = true;
		Path = strdup(&request_headers[4]);
	}
	else if (strncmp(request_headers, "HEAD ", 5) == 0)
	{
		get = false;
		Path = strdup(&request_headers[5]);
	}
	else
	{
		close(cfd);
		return;
	}

	char *path = Path;
	while(*path == '/')
		path++;

	char *pars = NULL;
	{
		char *dummy = strchr(path, '\r');
		if (!dummy)
			dummy = strchr(path, '\n');
		if (dummy)
			*dummy = 0x00;

		dummy = strchr(path, ' ');
		if (dummy)
			*dummy = 0x00;

		log(LL_DEBUG, "URL: %s", path);

		char *q = strchr(path, '?');
		if (q) {
			*q = 0x00;

			pars = un_url_escape(q + 1);
		}
	}

	if (strncmp(path, "rest/", 5) == 0) {
		log(LL_DEBUG, "%s", path);
		std::string reply = run_rest(cfg, &path[5], pars ? pars : "", snapshot_dir, quality);

		if (WRITE(cfd, reply.c_str(), reply.size()) <= 0)
			log(LL_DEBUG, "short write on response");
	}
	else {
		log(LL_INFO, "Path %s not found", path);

		if (WRITE(cfd, "HTTP/1.0 404\r\n\r\nReliquis non loquor", 4) <= 0)
			log(LL_DEBUG, "short write on response header");
	}

	free(Path);
	free(pars);

	close(cfd);
}

void send_file(const int cfd, const std::string & path, const char *const name, const bool dl)
{
	std::string complete_path = path + "/" + name;

	std::string type = "text/html";
	if (complete_path.size() >= 3) {
		std::string ext = complete_path.substr(complete_path.size() -3);

		if (ext == "avi")
			type = "video/x-msvideo";
		else if (ext == "mp4")
			type = "video/mp4";
		else if (ext == "png")
			type = "image/png";
		else if (ext == "jpg")
			type = "image/jpeg";
		else if (ext == "css")
			type = "text/css";
		else if (ext == "flv")
			type = "video/x-flv";
	}

	log(LL_WARNING, "Sending file %s of type %s", complete_path.c_str(), type.c_str());

	FILE *fh = fopen(complete_path.c_str(), "r");
	if (!fh) {
		log(LL_WARNING, "Cannot access %s: %s", complete_path.c_str(), strerror(errno));

		std::string headers = "HTTP/1.0 404 na\r\nServer: " NAME " " VERSION "\r\n\r\n";

		(void)WRITE(cfd, headers.c_str(), headers.size());

		return;
	}

	std::string name_header = dl ? myformat("Content-Disposition: attachment; filename=\"%s\"\r\n", name) : "";
	std::string headers = "HTTP/1.0 200 OK\r\nServer: " NAME " " VERSION "\r\n" + name_header + "Content-Type: " + type + "\r\n\r\n";
	(void)WRITE(cfd, headers.c_str(), headers.size());

	// FIXME
	while(!feof(fh)) {
		char buffer[4096];

		int rc = fread(buffer, 1, sizeof(buffer), fh);
		if (rc <= 0)
			break;

		if (WRITE(cfd, buffer, rc) <= 0) {
			log(LL_INFO, "Short write");
			break;
		}
	}

	fclose(fh);
}

bool sort_files_last_change(const file_t &left, const file_t & right)
{
	return left.last_change < right.last_change;
}

bool sort_files_size(const file_t &left, const file_t & right)
{
	return left.size < right.size;
}

bool sort_files_name(const file_t &left, const file_t & right)
{
	return left.name.compare(right.name) < 0;
}

const std::string http_200_header =
		"HTTP/1.0 200\r\n"
		"Cache-Control: no-cache\r\n"
		"Pragma: no-cache\r\n"
		"Server: " NAME " " VERSION "\r\n"
		"Expires: thu, 01 dec 1994 16:00:00 gmt\r\n"
		"Content-Type: text/html\r\n"
		"Connection: close\r\n"
		"\r\n";

const std::string html_header =
		"<!DOCTYPE html>"
		"<html>"
		"<head>"
		"<link href=\"stylesheet.css\" rel=\"stylesheet\" media=\"screen\">"
		"<link rel=\"shortcut icon\" href=\"favicon.ico\">"
		"<title>" NAME " " VERSION "</title>"
		"</head>"
		"<body>"
		"<h1>" NAME " " VERSION " / %s</h1>";

const std::string html_tail =
		"<p><br><br><br></p><hr><div id=\"tail\"><p>" NAME " was written by folkert@vanheusden.com</p></div>"
		"</body>"
		"</html>";

const std::string action_failed = "<h2>%s</h2><p>Action failed</p>";
const std::string action_succeeded = "<h2>%s</h2><p>Action succeeded</p>";

std::string http_server::mjpeg_stream_url(configuration_t *const cfg, const std::string & id)
{
	instance_t *inst = NULL;
	interface *i = NULL;
	find_by_id(cfg, id, &inst, &i);

	if (!inst)
		return "id not found";

	return myformat("stream.mjpeg?inst=%s", url_escape(inst -> name).c_str()); 
}

view *find_view(std::vector<view *> *const views, const std::string & id)
{
	if (!views)
		return NULL;

	for(view *v : *views) {
		if (v -> get_id() == id)
			return v;
	}

	return NULL;
}

void handle_http_client(int cfd, double fps, int quality, int time_limit, const std::vector<filter *> *const filters, std::atomic_bool *const global_stopflag, resize *const r, const int resize_w, const int resize_h, const bool motion_compatible, instance_t *inst, const std::string & snapshot_dir, const bool allow_admin, const bool archive_acces, configuration_t *const cfg, std::vector<view *> *const views)
{
	sigset_t all_sigs;
	sigfillset(&all_sigs);
	pthread_sigmask(SIG_BLOCK, &all_sigs, NULL);

	char request_headers[65536] = { 0 };
	int h_n = 0;

	struct pollfd fds[1] = { { cfd, POLLIN, 0 } };

	for(;!*global_stopflag && !motion_compatible;) {
		if (poll(fds, 1, 250) == 0)
			continue;

		int rc = read(cfd, &request_headers[h_n], sizeof request_headers - h_n - 1);
		if (rc == -1)
		{
			if (errno == EINTR || errno == EAGAIN)
				continue;

			log(LL_DEBUG, "error receiving request headers");
			close(cfd);
			return;
		}
		else if (rc == 0)
		{
			log(LL_DEBUG, "error receiving request headers");
			close(cfd);
			return;
		}

		h_n += rc;

		request_headers[h_n] = 0x00;

		if (strstr(request_headers, "\r\n\r\n") || strstr(request_headers, "\n\n"))
			break;
	}

	char *Path = NULL;

	bool get = true;
	if (motion_compatible) {
	}
	else if (strncmp(request_headers, "GET ", 4) == 0)
	{
		get = true;
		Path = strdup(&request_headers[4]);
	}
	else if (strncmp(request_headers, "HEAD ", 5) == 0)
	{
		get = false;
		Path = strdup(&request_headers[5]);
	}
	else
	{
		close(cfd);
		return;
	}

	char *path = Path;
	if (!motion_compatible) {
		while(*path == '/')
			path++;
	}

	std::map<std::string, std::string> pars;

	if (!motion_compatible) {
		char *dummy = strchr(path, '\r');
		if (!dummy)
			dummy = strchr(path, '\n');
		if (dummy)
			*dummy = 0x00;

		dummy = strchr(path, ' ');
		if (dummy)
			*dummy = 0x00;

		log(LL_DEBUG, "URL: %s", path);

		char *q = strchr(path, '?');
		if (q) {
			*q = 0x00;

			char *pars_str = un_url_escape(q + 1);

			char *p = pars_str;
			while(p) {
				std::string pair;
				char *end = strchr(p, '&');
				if (end) {
					pair = std::string(p, end - p);
					p = end + 1;
				}
				else {
					pair = std::string(p);
					p = NULL;
				}

				std::string key, value;

				ssize_t is = pair.find('=');
				if (is == std::string::npos)
					key = pair;
				else {
					key = pair.substr(0, is);
					value = pair.substr(is + 1);
				}

				pars.insert(std::pair<std::string, std::string>(key, value));
			}

			free(pars_str);

			if (!inst) {
				auto inst_it = pars.find("inst");

				if (inst_it != pars.end())
					inst = find_instance_by_name(cfg, inst_it -> second);
			}
		}
	}

	source *const s = inst ? find_source(inst) : NULL;
	if (s)
		s -> register_user();

	// iup = instance url parameter
	const std::string iup = inst ? myformat("?inst=%s", url_escape(inst -> name).c_str()) : "";

	//if (inst)
	//printf("%s\n", inst -> name.c_str());

	const std::string page_header = myformat(html_header.c_str(), inst ? inst -> name.c_str() : "(everything)");

	if ((path == NULL || strcmp(path, "stream.mjpeg") == 0 || motion_compatible) && s)
		send_mjpeg_stream(cfd, s, fps, quality, get, time_limit, filters, global_stopflag, r, resize_w, resize_h);
	else if (strcmp(path, "view-proxy") == 0 && views) {
		auto view_it = pars.find("id");
		std::string id;

		if (view_it != pars.end())
			id = view_it -> second;

		view *v = find_view(views, id);

		send_mjpeg_view_stream(cfd, v, fps, quality, get, time_limit, global_stopflag);
	}
	else if (strcmp(path, "stream.mpng") == 0 && s)
		send_mpng_stream(cfd, s, fps, get, time_limit, filters, global_stopflag, r, resize_w, resize_h);
	else if (strcmp(path, "image.png") == 0 && s)
		send_png_frame(cfd, s, get, filters, r, resize_w, resize_h);
	else if (strcmp(path, "image.jpg") == 0 && s)
		send_jpg_frame(cfd, s, get, quality, filters, r, resize_w, resize_h);
	else if (strcmp(path, "stylesheet.css") == 0) {
		struct stat st;
		if (stat("stylesheet.css", &st) == 0)
			send_file(cfd, "./", "stylesheet.css", false);
		else {
			std::string reply = http_200_header + "\r\n";

			if (WRITE(cfd, reply.c_str(), reply.size()) <= 0)
				log(LL_DEBUG, "short write on response");
		}
	}
	else if (strcmp(path, "stream.html") == 0)
	{
		std::string reply = http_200_header + page_header + myformat("<img src=\"stream.mjpeg%s\">", iup.c_str()) + html_tail;

		if (WRITE(cfd, reply.c_str(), reply.size()) <= 0)
			log(LL_DEBUG, "short write on response header");
	}
	else if (strcmp(path, "index.html") == 0 || strcmp(path, "") == 0)
	{
		std::string reply = "???";

		if (inst) {
			reply = http_200_header + page_header + "<div id=\"main\">"
				"<ul>" +
				myformat("<li><a href=\"stream.mjpeg%s\">MJPEG stream</a>"
						"<li><a href=\"stream.html%s\">Same MJPEG stream but in a HTML wrapper</a>"
						"<li><a href=\"stream.mpng%s\">MPNG stream</a>"
						"<li><a href=\"image.jpg%s\">Show snapshot in JPG format</a>"
						"<li><a href=\"image.png%s\">Show snapshot in PNG format</a>", iup.c_str(), iup.c_str(), iup.c_str(), iup.c_str(), iup.c_str());

			if (allow_admin) {
				reply += myformat("<li><a href=\"snapshot-img/%s\">Take a snapshot and store it on disk</a>"
						"<li><a href=\"snapshot-video/%s\">Start a video recording</a>", iup.c_str(), iup.c_str());
			}

			if (archive_acces) {
				reply += "<li><a href=\"view-snapshots/\">View recordings</a>";
			}

			reply += "</ul></div>";

			if (allow_admin) {
				cfg -> lock.lock();

				for(interface *i : inst -> interfaces)
					reply += describe_interface(inst, i);
				cfg -> lock.unlock();
			}

			reply += html_tail; 
		}
		else {
			reply = http_200_header + page_header + "<h2>views</h2>";

			if (!views || views -> empty())
				reply += "<b>No view defined</b>";
			else {
				reply += "<ul>";

				for(view *cv : *views) 
					reply += myformat("<li><a href=\"view-view?id=%s\">%s</a>", cv -> get_id().c_str(), (cv -> get_descr().empty() ? cv -> get_id() : cv -> get_descr()).c_str());

				reply += "</ul>";
			}
		}

		if (WRITE(cfd, reply.c_str(), reply.size()) <= 0)
			log(LL_DEBUG, "short write on response header");
	}
	else if ((strncmp(path, "view-snapshots/send-file", 24) == 0 || strncmp(path, "send-file", 9) == 0) && (archive_acces || allow_admin)) {
		auto file_it = pars.find("file");
		std::string file;

		if (file_it != pars.end())
			file = file_it -> second;

		bool force_dl = pars.find("download") != pars.end();

		if (!file.empty() && validate_file(snapshot_dir, file)) {
			send_file(cfd, snapshot_dir, file.c_str(), force_dl);
		}
		else {
			log(LL_WARNING, "%s is not a valid file", file.c_str());

			std::string reply = "HTTP/1.0 404\r\n\r\nfailed";

			if (WRITE(cfd, reply.c_str(), reply.size()) <= 0)
				log(LL_DEBUG, "short write on response header");
		}
	}
	else if (strcmp(path, "view-view") == 0 && views) {
		auto view_it = pars.find("id");
		std::string id;

		if (view_it != pars.end())
			id = view_it -> second;

		view *v = find_view(views, id);

		std::string reply;
		if (v)
			reply = http_200_header + v -> get_html();

		if (WRITE(cfd, reply.c_str(), reply.size()) <= 0)
			log(LL_DEBUG, "short write on response");
	}
	else if ((strncmp(path, "view-snapshots/view-file", 24) == 0 || strncmp(path, "view-file", 9) == 0) && (archive_acces || allow_admin)) {
		auto file_it = pars.find("file");
		std::string file;

		if (file_it != pars.end())
			file = file_it -> second;

		std::string reply = http_200_header + page_header + 
			myformat("<video controls autoplay><source src=\"send-file?file=%s\">Your browser does not support the video tag.</video>", file.c_str())
			+ html_tail;

		if (WRITE(cfd, reply.c_str(), reply.size()) <= 0)
			log(LL_DEBUG, "short write on response");
	}
	else if (strcmp(path, "view-snapshots/") == 0 && (archive_acces || allow_admin)) {
		std::string reply = http_200_header + page_header + "<h2>list of files in " + snapshot_dir + "</h2><table border=1>";

		auto *files = load_filelist(snapshot_dir, "");

		auto order_it = pars.find("order");
		std::string order;

		if (order_it != pars.end())
			order = order_it -> second;

		if (order == "" || order == "last-change")
			std::sort(files -> begin(), files -> end(), sort_files_last_change);
		else if (order == "name")
			std::sort(files -> begin(), files -> end(), sort_files_name);
		else if (order == "size")
			std::sort(files -> begin(), files -> end(), sort_files_size);
		else
			reply += myformat("Ignoring unknown sort method %s", order.c_str());

		reply += "<tr><th><a href=\"?order=name\">name</a></th><th><a href=\"?order=last-change\">last change</a></th><th><a href=\"?order=size\">size</a></th><th></th></tr>";

		for(auto file : *files) {
			reply += std::string("<tr><td><a href=\"send-file?download&file=") + file.name + "\">" + file.name + "</a></td><td>" + myctime(file.last_change) + "</td><td>" + myformat("%zu", file.size) + "</td><td>" + myformat("<a href=\"view-file?file=%s\">view</a>", file.name.c_str()) + "</td></tr>";
		}

		delete files;

		reply += "</table>" + html_tail;

		if (WRITE(cfd, reply.c_str(), reply.size()) <= 0)
			log(LL_DEBUG, "short write on response header");
	}
	else if (strncmp(path, "favicon.ico", 11) == 0) {
		struct stat st;
		if (stat("favicon.ico", &st) == 0)
			send_file(cfd, "./", "favicon.ico", false);
		else {
			std::string reply = http_200_header + "\r\n";

			if (WRITE(cfd, reply.c_str(), reply.size()) <= 0)
				log(LL_DEBUG, "short write on response header");
		}
	}
	else if (!allow_admin) {
		goto do404;
	}
	else if (strcmp(path, "pause") == 0 || strcmp(path, "unpause") == 0) {
		std::string reply = http_200_header + "???";

		auto mod_it = pars.find("module");
		std::string module;

		if (mod_it != pars.end())
			module = mod_it -> second;

		cfg -> lock.lock();
		if (pause(inst, module, strcmp(path, "pause") == 0))
			reply = http_200_header + page_header + myformat(action_succeeded.c_str(), "pause/unpause") + html_tail;
		else
			reply = http_200_header + page_header + myformat(action_failed.c_str(), "pause/unpause") + html_tail;
		cfg -> lock.unlock();

		if (WRITE(cfd, reply.c_str(), reply.size()) <= 0)
			log(LL_DEBUG, "short write on response header");
	}
	else if (strcmp(path, "start") == 0 || strcmp(path, "stop") == 0) {
		std::string reply = http_200_header + "???";

		auto mod_it = pars.find("module");
		std::string module;

		if (mod_it != pars.end())
			module = mod_it -> second;

		cfg -> lock.lock();
		interface *i = find_by_id(inst, module);
		//printf("inst %s: %p\n", module.c_str(), i);

		if (!i)
			reply = "???";
		else if (start_stop(i, strcmp(path, "start") == 0))
			reply = http_200_header + page_header + myformat(action_succeeded.c_str(), "start/stop") + html_tail;
		else
			reply = http_200_header + page_header + myformat(action_failed.c_str(), "start/stop") + html_tail;
		cfg -> lock.unlock();

		if (WRITE(cfd, reply.c_str(), reply.size()) <= 0)
			log(LL_DEBUG, "short write on response header");
	}
	else if (strcmp(path, "restart") == 0) {
		std::string reply = http_200_header + "???";

		auto mod_it = pars.find("module");
		std::string module;

		if (mod_it != pars.end())
			module = mod_it -> second;

		cfg -> lock.lock();
		interface *i = find_by_id(inst, module);

		if (!i)
			reply = "???";
		else if (start_stop(i, false)) {
			if (start_stop(i, true))
				reply = http_200_header + page_header + myformat(action_succeeded.c_str(), "restart") + html_tail;
			else
				reply = http_200_header + page_header + myformat(action_failed.c_str(), "restart") + html_tail;
		}
		cfg -> lock.unlock();

		if (WRITE(cfd, reply.c_str(), reply.size()) <= 0)
			log(LL_DEBUG, "short write on response header");
	}
	else if (strcmp(path, "snapshot-img/") == 0) {
		std::string reply = http_200_header + "???";

		if (take_a_picture(s, snapshot_dir, quality))
			reply = http_200_header + page_header + myformat(action_succeeded.c_str(), "snapshot image") + html_tail;
		else
			reply = http_200_header + page_header + myformat(action_failed.c_str(), "snapshot image") + html_tail;

		if (WRITE(cfd, reply.c_str(), reply.size()) <= 0)
			log(LL_DEBUG, "short write on response header");
	}
	else if (strcmp(path, "snapshot-video/") == 0) {
		interface *i = start_a_video(s, snapshot_dir, quality);
		//printf("interface %p instance %p\n", i, inst);

		std::string reply = http_200_header + "???";
		if (i) {
			reply = http_200_header + page_header + myformat(action_succeeded.c_str(), "snapshot video") + html_tail;

			cfg -> lock.lock();
			inst -> interfaces.push_back(i);
			cfg -> lock.unlock();
		}
		else {
			reply = http_200_header + page_header + myformat(action_failed.c_str(), "snapshot video") + html_tail;
		}

		if (WRITE(cfd, reply.c_str(), reply.size()) <= 0)
			log(LL_DEBUG, "short write on response header");
	}
	else {
do404:
		log(LL_INFO, "Path %s not found", path);

		if (WRITE(cfd, "HTTP/1.0 404\r\n\r\nwhat?", 4) <= 0)
			log(LL_DEBUG, "short write on response header");
	}

	free(Path);

	close(cfd);

	if (s)
		s -> unregister_user();
}

void * handle_http_client_thread(void *ct_in)
{
	http_thread_t *ct = (http_thread_t *)ct_in;

	set_thread_name("http_client");

	if (ct -> is_rest) {
		handle_rest(ct -> fd, ct -> cfg, ct -> global_stopflag, ct -> snapshot_dir, ct -> quality);
	}
	else {
		handle_http_client(ct -> fd, ct -> fps, ct -> quality, ct -> time_limit, ct -> filters, ct -> global_stopflag, ct -> r, ct -> resize_w, ct -> resize_h, ct -> motion_compatible, ct -> inst, ct -> snapshot_dir, ct -> allow_admin, ct -> archive_acces, ct -> cfg, ct -> views);
	}

	delete ct;

	return NULL;
}

http_server::http_server(configuration_t *const cfg, const std::string & id, const std::string & descr, instance_t *const inst, const std::string & http_adapter, const int http_port, const double fps, const int quality, const int time_limit, const std::vector<filter *> *const f, resize *const r, const int resize_w, const int resize_h, const bool motion_compatible, const bool allow_admin, const bool archive_acces, const std::string & snapshot_dir, const bool is_rest, std::vector<view *> *const views) : cfg(cfg), interface(id, descr), inst(inst), fps(fps), quality(quality), time_limit(time_limit), f(f), r(r), resize_w(resize_w), resize_h(resize_h), motion_compatible(motion_compatible), allow_admin(allow_admin), archive_acces(archive_acces), snapshot_dir(snapshot_dir), is_rest(is_rest), views(views)
{
	fd = start_listen(http_adapter.c_str(), http_port, 5);
	ct = CT_HTTPSERVER;
}

http_server::~http_server()
{
	close(fd);

	free_filters(f);

	delete f;
}

void http_server::operator()()
{
	set_thread_name("http_server");

	struct pollfd fds[] = { { fd, POLLIN, 0 } };

	std::vector<pthread_t> handles;

	for(;!local_stop_flag;) {
		for(size_t i=0; i<handles.size();) {
			if (pthread_tryjoin_np(handles.at(i), NULL) == 0)
				handles.erase(handles.begin() + i);
			else
				i++;
		}

		get_meta() -> set_int("$http-viewers$", std::pair<uint64_t, int>(0, handles.size()));

		pauseCheck();

		if (poll(fds, 1, 500) == 0)
			continue;

		int cfd = accept(fd, NULL, 0);
		if (cfd == -1)
			continue;

		log(id, LL_INFO, "HTTP connected with: %s", get_endpoint_name(cfd).c_str());

		http_thread_t *ct = new http_thread_t;

		ct -> inst = inst;
		ct -> fd = cfd;
		ct -> fps = fps;
		ct -> quality = quality;
		ct -> time_limit = time_limit;
		ct -> filters = f;
		ct -> global_stopflag = &local_stop_flag;
		ct -> r = r;
		ct -> resize_w = resize_w;
		ct -> resize_h = resize_h;
		ct -> motion_compatible = motion_compatible;
		ct -> cfg = cfg;
		ct -> snapshot_dir = snapshot_dir;
		ct -> allow_admin = allow_admin;
		ct -> archive_acces = archive_acces;
		ct -> is_rest = is_rest;
		ct -> views = views;

		pthread_t th;
		int rc = -1;
		if ((rc = pthread_create(&th, NULL, handle_http_client_thread, ct)) != 0)
		{
			errno = rc;
			error_exit(true, "pthread_create failed (http server)");
		}

		handles.push_back(th);
	}

	for(size_t i=0; i<handles.size(); i++)
		pthread_join(handles.at(i), NULL);

	log(id, LL_INFO, "HTTP server thread terminating");
}
