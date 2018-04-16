// (C) 2017 by folkert van heusden, released under AGPL v3.0
#include <dlfcn.h>
#include <libconfig.h++>
#include <set>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include <curl/curl.h>

using namespace libconfig;

#include "config.h"
#include "error.h"
#include "utils.h"
#include "source.h"
#include "source_v4l.h"
#include "source_http_jpeg.h"
#include "source_http_mjpeg.h"
#include "source_rtsp.h"
#include "source_plugin.h"
#include "http_server.h"
#include "motion_trigger.h"
#include "target.h"
#ifdef WITH_GWAVI
#include "target_avi.h"
#endif
#include "target_ffmpeg.h"
#include "target_jpeg.h"
#include "target_plugin.h"
#include "target_extpipe.h"
#include "target_vnc.h"
#include "filter.h"
#include "filter_mirror_v.h"
#include "filter_mirror_h.h"
#include "filter_noise_neighavg.h"
#include "filter_apply_mask.h"
#include "filter_add_text.h"
#include "filter_add_scaled_text.h"
#include "filter_grayscale.h"
#include "filter_boost_contrast.h"
#include "filter_marker_simple.h"
#include "v4l2_loopback.h"
#include "filter_overlay.h"
#include "picio.h"
#include "filter_plugin.h"
#include "log.h"
#include "cfg.h"
#include "resize_cairo.h"
#include "resize_crop.h"
#include "filter_motion_only.h"
#include "selection_mask.h"

std::string cfg_str(const Config & cfg, const char *const key, const char *descr, const bool optional, const std::string & def)
{
	try {
		return cfg.lookup(key);
	}
	catch(const SettingNotFoundException &nfex) {
		if (!optional)
			error_exit(false, "\"%s\" not found (%s)", key, descr);
	}

	log(LL_DEBUG, "\"%s\" not found (%s), assuming default (%s)", key, descr, def.c_str());

	return def; // field is optional
}

std::string cfg_str(const Setting & cfg, const char *const key, const char *descr, const bool optional, const std::string & def)
{
	std::string v = def;

	try {
		v = (const char *)cfg.lookup(key);
	}
	catch(const SettingNotFoundException &nfex) {
		if (!optional)
			error_exit(false, "\"%s\" not found (%s)", key, descr);

		log(LL_DEBUG, "\"%s\" not found (%s), assuming default (%s)", key, descr, def.c_str());
	}

	catch(const SettingTypeException & ste) {
		error_exit(false, "Expected a string value for \"%s\" (%s) at line %d but got something else", key, descr, cfg.getSourceLine());
	}

	return v;
}

double cfg_float(const Setting & cfg, const char *const key, const char *descr, const bool optional, const double def=-1.0)
{
	double v = def;

	try {
		v = cfg.lookup(key);
	}
	catch(const SettingNotFoundException &nfex) {
		if (!optional)
			error_exit(false, "\"%s\" not found (%s)", key, descr);

		log(LL_DEBUG, "\"%s\" not found (%s), assuming default (%f)", key, descr, def);
	}

	catch(const SettingTypeException & ste) {
		error_exit(false, "Expected a float value for \"%s\" (%s) at line %d but got something else (did you forget to add \".0\"?)", key, descr, cfg.getSourceLine());
	}

	return v;
}

int cfg_int(const Setting & cfg, const char *const key, const char *descr, const bool optional, const int def=-1)
{
	int v = def;

	try {
		v = cfg.lookup(key);
	}
	catch(const SettingNotFoundException &nfex) {
		if (!optional)
			error_exit(false, "\"%s\" not found (%s)", key, descr);

		log(LL_DEBUG, "\"%s\" not found (%s), assuming default (%f)", key, descr, def);
	}

	catch(const SettingTypeException & ste) {
		error_exit(false, "Expected an int value for \"%s\" (%s) at line %d but got something else", key, descr, cfg.getSourceLine());
	}

	return v;
}

int cfg_bool(const Setting & cfg, const char *const key, const char *descr, const bool optional, const bool def=false)
{
	bool v = def;

	try {
		v = cfg.lookup(key);
	}
	catch(const SettingNotFoundException &nfex) {
		if (!optional)
			error_exit(false, "\"%s\" not found (%s)", key, descr);

		log(LL_DEBUG, "\"%s\" not found (%s), assuming default (%f)", key, descr, def);
	}

	catch(const SettingTypeException & ste) {
		error_exit(false, "Expected a boolean value for \"%s\" (%s) at line %d but got something else", key, descr, cfg.getSourceLine());
	}

	return v;
}

void add_filters(std::vector<filter *> *af, const std::vector<filter *> *const in)
{
	for(filter *f : *in)
		af -> push_back(f);
}

bool find_interval_or_fps(const Setting & cfg, double *const interval, const std::string & fps_name, double *const fps)
{
	bool have_interval = cfg.lookupValue("interval", *interval);
	bool have_fps = cfg.lookupValue(fps_name.c_str(), *fps);

	if (!have_interval && !have_fps)
		return false;

	if (have_interval && have_fps)
		return false;

	if (have_interval) {
		if (*interval == 0)
			return false;

		if (*interval < 0)
			*fps = -1.0;
		else
			*fps = 1.0 / *interval;
	}
	else {
		if (*fps == 0)
			return false;

		if (*fps <= 0)
			*interval = -1.0;
		else
			*interval = 1.0 / *fps;
	}

	return true;
}

std::vector<filter *> *load_filters(const Setting & in, source *const s, resize *const r)
{
	std::vector<filter *> *const filters = new std::vector<filter *>();

	size_t n_f = in.getLength();
	log(LL_DEBUG, " %zu filters", n_f);

	for(size_t i=0; i<n_f; i++) {
		const Setting & ae = in[i];

		std::string s_type = cfg_str(ae, "type", "filter-type (h-mirror, marker, etc)", false, "");
		if (s_type == "null")
			filters -> push_back(new filter());
		else if (s_type == "h-mirror")
			filters -> push_back(new filter_mirror_h());
		else if (s_type == "v-mirror")
			filters -> push_back(new filter_mirror_v());
		else if (s_type == "neighbour-noise-filter")
			filters -> push_back(new filter_noise_neighavg());
		else if (s_type == "grayscale")
			filters -> push_back(new filter_grayscale());
		else if (s_type == "filter-plugin") {
			std::string file = cfg_str(ae, "file", "filename of filter plugin", false, "");
			std::string par = cfg_str(ae, "par", "parameter for filter plugin", false, "");

			filters -> push_back(new filter_plugin(file, par));
		}
		else if (s_type == "marker") {
			std::string s_position = cfg_str(ae, "mode", "marker mode, e.g. red, red-invert or invert", false, "red");

			sm_mode_t sm = m_red;

			if (s_position == "red")
				sm = m_red;
			else if (s_position == "red-invert")
				sm = m_red_invert;
			else if (s_position == "invert")
				sm = m_invert;

			std::string selection_bitmap = cfg_str(ae, "selection-bitmap", "bitmaps indicating which pixels to look at. must be same size as webcam image and must be a .pbm-file. leave empty to disable.", true, "");
			selection_mask *psm = selection_bitmap.empty() ? NULL : new selection_mask(r, selection_bitmap);
			int noise_level = cfg_int(ae, "noise-factor", "at what difference levell is the pixel considered to be changed", true, 32);
			double pixels_changed_perctange = cfg_float(ae, "pixels-changed-percentage", "what %% of pixels need to be changed before the marker is drawn", false, 1.0);

			filters -> push_back(new filter_marker_simple(sm, psm, s -> get_meta(), noise_level, pixels_changed_perctange));
		}
		else if (s_type == "apply-mask") {
			std::string selection_bitmap = cfg_str(ae, "selection-bitmap", "bitmaps indicating which pixels to look at. must be same size as webcam image and must be a .pbm-file. leave empty to disable.", false, "");
			selection_mask *sm = selection_bitmap.empty() ? NULL : new selection_mask(r, selection_bitmap);

			filters -> push_back(new filter_apply_mask(sm));
		}
		else if (s_type == "motion-only") {
			std::string selection_bitmap = cfg_str(ae, "selection-bitmap", "bitmaps indicating which pixels to look at. must be same size as webcam image and must be a .pbm-file. leave empty to disable.", true, "");
			selection_mask *sm = selection_bitmap.empty() ? NULL : new selection_mask(r, selection_bitmap);
			int noise_level = cfg_int(ae, "noise-factor", "at what difference levell is the pixel considered to be changed", true, 32);

			filters -> push_back(new filter_motion_only(sm, noise_level));
		}
		else if (s_type == "boost-contrast")
			filters -> push_back(new filter_boost_contrast());
		else if (s_type == "overlay") {
			std::string s_pic = cfg_str(ae, "picture", "PNG to overlay (with alpha channel!)", false, "");
			int x = cfg_int(ae, "x", "x-coordinate of overlay", false, 0);
			int y = cfg_int(ae, "y", "y-coordinate of overlay", false, 0);

			filters -> push_back(new filter_overlay(s_pic, x, y));
		}
		else if (s_type == "text") {
			std::string s_text = cfg_str(ae, "text", "what text to show", false, "");
			std::string s_position = cfg_str(ae, "position", "where to put it, e.g. upper-left, lower-center, center-right", false, "");

			text_pos_t tp = center_center;

			if (s_position == "upper-left")
				tp = upper_left;
			else if (s_position == "upper-center")
				tp = upper_center;
			else if (s_position == "upper-right")
				tp = upper_right;
			else if (s_position == "center-left")
				tp = center_left;
			else if (s_position == "center-center")
				tp = center_center;
			else if (s_position == "center-right")
				tp = center_right;
			else if (s_position == "lower-left")
				tp = lower_left;
			else if (s_position == "lower-center")
				tp = lower_center;
			else if (s_position == "lower-right")
				tp = lower_right;
			else
				error_exit(false, "(text-)position %s is not understood", s_position.c_str());

			filters -> push_back(new filter_add_text(s_text, tp, s));
		}
		else if (s_type == "scaled-text") {
			std::string s_text = cfg_str(ae, "text", "what text to show", false, "");
			std::string font = cfg_str(ae, "font", "which font to use", false, "");
			int x = cfg_int(ae, "x", "x-coordinate of text", false, 0);
			int y = cfg_int(ae, "y", "y-coordinate of text", false, 0);
			int fs = cfg_int(ae, "font-size", "font size (in pixels)", false, 10);
			int r = cfg_int(ae, "r", "red component of text color", true, 0);
			int g = cfg_int(ae, "g", "green component of text color", true, 0);
			int b = cfg_int(ae, "b", "blue component of text color", true, 0);

			filters -> push_back(new filter_add_scaled_text(s_text, font, x, y, fs, r, g, b, s));
		}
		else {
			error_exit(false, "Filter %s is not known", s_type.c_str());
		}
	}

	return filters;
}

stream_plugin_t * load_stream_plugin(const Setting & in)
{
	std::string file = cfg_str(in, "stream-writer-plugin-file", "filename of stream writer plugin", false, "");
	if (file.empty())
		return NULL;

	stream_plugin_t *sp = new stream_plugin_t;

	sp -> par = cfg_str(in, "stream-writer-plugin-parameter", "parameter for stream writer plugin", true, "");

	void *library = dlopen(file.c_str(), RTLD_NOW);
	if (!library)
		error_exit(true, "Failed opening stream writer library %s", file.c_str());

	sp -> init_plugin = (tp_init_plugin_t)find_symbol(library, "init_plugin", "stream writer plugin", file.c_str());
	sp -> open_file = (open_file_t)find_symbol(library, "open_file", "stream writer plugin", file.c_str());
	sp -> write_frame = (write_frame_t)find_symbol(library, "write_frame", "stream writer plugin", file.c_str());
	sp -> close_file = (close_file_t)find_symbol(library, "close_file", "stream writer plugin", file.c_str());
	sp -> uninit_plugin = (tp_uninit_plugin_t)find_symbol(library, "uninit_plugin", "stream writer plugin", file.c_str());

	return sp;
}

void interval_fps_error(const char *const name, const char *what, const char *id)
{
	error_exit(false, "Interval/%s %s not set or invalid (e.g. 0) for target (%s). Make sure that you use a float-value for the fps/interval, e.g. 13.0 instead of 13", name, what, id);
}

target * load_target(const Setting & in, source *const s, resize *const r)
{
	target *t = NULL;

	std::string id = cfg_str(in, "id", "some identifier; visible in e.g. the http server", true, "");

	std::string path = cfg_str(in, "path", "directory to write to", false, "");
	std::string prefix = cfg_str(in, "prefix", "string to begin filename with", true, "");

	std::string exec_start = cfg_str(in, "exec-start", "script to start when recording starts", true, "");
	std::string exec_cycle = cfg_str(in, "exec-cycle", "script to start when the record file is restarted", true, "");
	std::string exec_end = cfg_str(in, "exec-end", "script to start when the recording stops", true, "");

	int restart_interval = cfg_int(in, "restart-interval", "after how many seconds should the stream-file be restarted", true, -1);

#ifdef WITH_GWAVI
	std::string format = cfg_str(in, "format", "avi, extpipe, ffmpeg (for mp4, flv, etc), jpeg, vnc or plugin", false, "");
#else
	std::string format = cfg_str(in, "format", "extpipe, ffmpeg (for mp4, flv, etc), jpeg, vnc or plugin", false, "");
#endif
	int jpeg_quality = cfg_int(in, "quality", "JPEG quality, this influences the size", true, 75);

	double fps = 0, interval = 0;
	if (!find_interval_or_fps(in, &interval, "fps", &fps))
		interval_fps_error("fps", "for writing frames to disk (FPS as the video frames are retrieved)", id.c_str());

	double override_fps = cfg_float(in, "override-fps", "What FPS to use in the output file. That way you can let the resulting file be played faster (or slower) than how the stream is obtained from the camera. -1.0 to disable", true, -1.0);

	std::vector<filter *> *filters = NULL;
	try {
		const Setting & f = in.lookup("filters");
		filters = load_filters(f, s, r);
	}
	catch(SettingNotFoundException & snfe) {
	}

	if (format == "avi")
#ifdef WITH_GWAVI
		t = new target_avi(id, s, path, prefix, jpeg_quality, restart_interval, interval, filters, exec_start, exec_cycle, exec_end, override_fps);
#else
		error_exit(false, "libgwavi not compiled in");
#endif
	else if (format == "extpipe") {
		std::string cmd = cfg_str(in, "cmd", "Command to send the frames to", false, "");

		t = new target_extpipe(id, s, path, prefix, jpeg_quality, restart_interval, interval, filters, exec_start, exec_cycle, exec_end, cmd);
	}
	else if (format == "ffmpeg") {
		int bitrate = cfg_int(in, "bitrate", "How many bits per second to emit. For 352x288 200000 is a sane value. This value affects the quality.", true, 200000);
		std::string type = cfg_str(in, "ffmpeg-type", "E.g. flv, mp4", true, "mp4");
		std::string const parameters = cfg_str(in, "ffmpeg-parameters", "Parameters specific for ffmpeg.", true, "");

		t = new target_ffmpeg(id, parameters, s, path, prefix, restart_interval, interval, type, bitrate, filters, exec_start, exec_cycle, exec_end, override_fps);
	}
	else if (format == "jpeg")
		t = new target_jpeg(id, s, path, prefix, jpeg_quality, restart_interval, interval, filters, exec_start, exec_cycle, exec_end);
	else if (format == "plugin") {
		stream_plugin_t *sp = load_stream_plugin(in);

		t = new target_plugin(id, s, path, prefix, jpeg_quality, restart_interval, interval, filters, exec_start, exec_cycle, exec_end, sp, override_fps);
	}
	else if (format == "vnc") {
		std::string listen_adapter = cfg_str(in, "listen-adapter", "network interface to listen on or 0.0.0.0 for all", false, "");
		int listen_port = cfg_int(in, "listen-port", "port to listen on", false, 5901);

		t = new target_vnc(id, s, listen_adapter, listen_port, restart_interval, interval, filters, exec_start, exec_end);
	}
	else {
		error_exit(false, "Format %s is unknown (stream to disk backends)", format.c_str());
	}

	return t;
}

void version()
{
	printf(NAME " " VERSION "\n");
	printf("(C) 2017 by Folkert van Heusden\n\n");
}

void help()
{
	printf("-c x   select configuration file\n");
	printf("-f     fork into the background\n");
	printf("-p x   file to write PID to\n");
	printf("-v     enable verbose mode\n");
	printf("-V     show version & exit\n");
	printf("-h     this help\n");
}

int main(int argc, char *argv[])
{
	const char *pid_file = NULL;
	const char *cfg_file = "constatus.cfg";
	bool do_fork = false, verbose = false;
	int ll = LL_INFO;

	int c = -1;
	while((c = getopt(argc, argv, "c:p:fhvV")) != -1) {
		switch(c) {
			case 'c':
				cfg_file = optarg;
				break;

			case 'p':
				pid_file = optarg;
				break;

			case 'f':
				do_fork = true;
				break;

			case 'h':
				help();
				return 0;

			case 'v':
				verbose = true;
				ll++;
				break;

			case 'V':
				version();
				return 0;

			default:
				help();
				return 1;
		}
	}

	signal(SIGCHLD, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);

	log(LL_INFO, "Loading %s...", cfg_file);

	curl_global_init(CURL_GLOBAL_ALL);

	Config lc_cfg;
	try
	{
		lc_cfg.readFile(cfg_file);
	}
	catch(const FileIOException &fioex)
	{
		error_exit(true, "I/O error while reading configuration file %s", cfg_file);
		return EXIT_FAILURE;
	}
	catch(const ParseException &pex)
	{
		error_exit(true, "Configuration file %s parse error at line %d: %s", pex.getFile(), pex.getLine(), pex.getError());
		return EXIT_FAILURE;
	}

	const Setting& root = lc_cfg.getRoot();

	//***

	std::string logfile = cfg_str(lc_cfg, "logfile", "file where to store logging", false, "");

	int loglevel = -1;

	if (verbose)
		loglevel = ll;
	else {
		std::string level = cfg_str(lc_cfg, "log-level", "log level (fatal, error, warning, info, debug, debug-verbose)", false, "");
		const char *ll = level.c_str();
		if (strcasecmp(ll, "fatal") == 0)
			loglevel = LL_FATAL;
		else if (strcasecmp(ll, "error") == 0)
			loglevel = LL_ERR;
		else if (strcasecmp(ll, "warning") == 0)
			loglevel = LL_WARNING;
		else if (strcasecmp(ll, "info") == 0)
			loglevel = LL_INFO;
		else if (strcasecmp(ll, "debug") == 0)
			loglevel = LL_DEBUG;
		else if (strcasecmp(ll, "debug-verbose") == 0)
			loglevel = LL_DEBUG_VERBOSE;
		else
			error_exit(false, "Log level '%s' not recognized", ll);

	}
	setlogfile(logfile.empty() ? NULL : logfile.c_str(), loglevel);

	log(LL_INFO, " *** " NAME " v" VERSION " starting ***");

	configuration_t cfg;
	cfg.lock.lock();

	std::string resize_type = cfg_str(lc_cfg, "resize-type", "can be regular, crop or cairo. This selects what method will be used for resizing the video stream (if requested).", true, "");

	resize *r = NULL;
	if (resize_type == "cairo") {
		r = new resize_cairo();
	}
	else if (resize_type == "regular" || resize_type == "") {
		r = new resize();

		if (resize_type == "")
			log(LL_INFO, "No resizer/scaler selected (\"resize-type\"); assuming default");
	}
	else if (resize_type == "crop") {
		r = new resize_crop();
	}
	else {
		error_exit(false, "Scaler/resizer of type \"%s\" is not known", resize_type.c_str());
	}

	const Setting &o_instances = root["instances"];
	size_t n_instances = o_instances.getLength();

	if (n_instances == 0)
		log(LL_WARNING, " 0 instances, is that correct? Note that since version 2.0 the configuration file layout changed a tiny bit to accomodate multiple instances!");

	for(size_t i_loop=0; i_loop<n_instances; i_loop++) {
		const Setting &instance_root = o_instances[i_loop];
		instance_t *ci = new instance_t; // ci = current instance

		//***
		const Setting &o_source = instance_root["source"];

		source *s = NULL;

		try
		{
			log(LL_INFO, "Configuring the video-source...");
			const std::string s_type = cfg_str(o_source, "type", "source-type", false, "");
			const std::string id = cfg_str(o_source, "id", "some identifier; visible in e.g. the http server", true, "");

			double max_fps = cfg_float(o_source, "max-fps", "limit the number of frames per second acquired to this value or -1.0 to disable", true, -1.0);
			if (max_fps == 0)
				error_exit(false, "Video-source: max-fps must be either > 0 or -1.0. Use -1.0 for no FPS limit.");

			int resize_w = cfg_int(o_source, "resize-width", "resize picture width to this (-1 to disable)", true, -1);
			int resize_h = cfg_int(o_source, "resize-height", "resize picture height to this (-1 to disable)", true, -1);

			double timeout = cfg_float(o_source, "timeout", "how long to wait for the camera before considering it be offline (in seconds)", true, 1.0);

			if (s_type == "v4l") {
				bool pref_jpeg = cfg_bool(o_source, "prefer-jpeg", "if the camera is capable of JPEG, should that be used", true, false);
				bool rpi_wa = cfg_bool(o_source, "rpi-workaround", "the raspberry pi camera has a bug, this enables a workaround", true, false);
				int w = cfg_int(o_source, "width", "width of picture", false);
				int h = cfg_int(o_source, "height", "height of picture", false);
				int jpeg_quality = cfg_int(o_source, "quality", "JPEG quality, this influences the size", true, 75);
				std::string dev = cfg_str(o_source, "device", "linux v4l2 device", false, "/dev/video0");

				s = new source_v4l(id, dev, pref_jpeg, rpi_wa, jpeg_quality, max_fps, w, h, r, resize_w, resize_h, loglevel, timeout);
			}
			else if (s_type == "jpeg") {
				bool ign_cert = cfg_bool(o_source, "ignore-cert", "ignore SSL errors", true, false);
				const std::string auth = cfg_str(o_source, "http-auth", "HTTP authentication string", true, "");
				const std::string url = cfg_str(o_source, "url", "address of JPEG stream", false, "");

				s = new source_http_jpeg(id, url, ign_cert, auth, max_fps, r, resize_w, resize_h, loglevel, timeout);
			}
			else if (s_type == "mjpeg") {
				const std::string url = cfg_str(o_source, "url", "address of MJPEG stream", false, "");
				bool ign_cert = cfg_bool(o_source, "ignore-cert", "ignore SSL errors", true, false);

				s = new source_http_mjpeg(id, url, ign_cert, max_fps, r, resize_w, resize_h, loglevel, timeout);
			}
			else if (s_type == "rtsp") {
				const std::string url = cfg_str(o_source, "url", "address of JPEG stream", false, "");
				bool tcp = cfg_bool(o_source, "tcp", "use TCP for RTSP transport (instead of default UDP)", true, false);

				s = new source_rtsp(id, url, tcp, max_fps, r, resize_w, resize_h, loglevel, timeout);
			}
			else if (s_type == "plugin") {
				std::string plugin_bin = cfg_str(o_source, "source-plugin-file", "filename of video data source plugin", true, "");
				std::string plugin_arg = cfg_str(o_source, "source-plugin-parameter", "parameter for video data source plugin", true, "");

				s = new source_plugin(id, plugin_bin, plugin_arg, max_fps, r, resize_w, resize_h, loglevel, timeout);
			}
			else {
				log(LL_FATAL, " no source defined!");
			}
		}
		catch(SettingException & se) {
			error_exit(false, "Error in %s", se.getPath());
		}

		//***
		log(LL_INFO, "Configuring the meta sources...");

		try {
			const Setting &ms = instance_root["meta-plugin"];
			size_t n_ms = ms.getLength();

			log(LL_DEBUG, " %zu meta plugin(s)", n_ms);

			if (n_ms == 0)
				log(LL_WARNING, " 0 plugins, is that correct?");

			for(size_t i=0; i<n_ms; i++) {
				const Setting &server = ms[i];

				std::string file = cfg_str(server, "plugin-file", "filename of the meta plugin", false, "");
				std::string par = cfg_str(server, "plugin-parameter", "parameter of the meta plugin", true, "");

				s -> get_meta() -> add_plugin(file, par.c_str());
			}
		}
		catch(const SettingNotFoundException &nfex) {
			log(LL_INFO, " no meta source");
		}

		// listen adapter, listen port, source, fps, jpeg quality, time limit (in seconds)
		log(LL_INFO, "Configuring the HTTP server(s)...");

		try {
			const Setting &hs = instance_root["http-server"];
			size_t n_hl = hs.getLength();

			log(LL_DEBUG, " %zu http server(s)", n_hl);

			if (n_hl == 0)
				log(LL_WARNING, " 0 servers, is that correct?");

			for(size_t i=0; i<n_hl; i++) {
				const Setting &server = hs[i];

				const std::string id = cfg_str(server, "id", "some identifier; visible in e.g. the http server", true, "");

				std::string listen_adapter = cfg_str(server, "listen-adapter", "network interface to listen on or 0.0.0.0 for all", false, "");
				int listen_port = cfg_int(server, "listen-port", "port to listen on", false, 8080);
				log(LL_INFO, " HTTP server listening on %s:%d", listen_adapter.c_str(), listen_port);

				int jpeg_quality = cfg_int(server, "quality", "JPEG quality, this influences the size", true, 75);
				int time_limit = cfg_int(server, "time-limit", "how long (in seconds) to stream before the connection is closed", true, -1);

				int resize_w = cfg_int(server, "resize-width", "resize picture width to this (-1 to disable)", true, -1);
				int resize_h = cfg_int(server, "resize-height", "resize picture height to this (-1 to disable)", true, -1);

				bool local_instance = cfg_bool(server, "local-instance", "show only parameters related to this instance", true, false);
				bool motion_compatible = cfg_bool(server, "motion-compatible", "only stream MJPEG and do not wait for HTTP request", true, false);
				bool allow_admin = cfg_bool(server, "allow-admin", "when enabled, you can partially configure services", true, false);
				bool is_rest = cfg_bool(server, "is-rest", "when enabled then this is a REST (only!) interface", true, false);
				bool archive_access = cfg_bool(server, "archive-access", "when enabled, you can retrieve recorded video/images", true, false);

				std::string snapshot_dir = cfg_str(server, "snapshot-dir", "where to store snapshots (triggered by HTTP server). see \"allow-admin\".", true, "");

				std::vector<filter *> *http_filters = NULL;
				try {
					const Setting & f = server.lookup("filters");
					http_filters = load_filters(f, s, r);
				}
				catch(SettingNotFoundException & snfe) {
				}

				double fps = 0, interval = 0;
				if (!find_interval_or_fps(server, &interval, "fps", &fps) && is_rest == false)
					interval_fps_error("fps", "for showing video frames", id.c_str());

				instance_t *look_at = local_instance ? ci : NULL;

				interface *h = new http_server(&cfg, id, look_at, listen_adapter, listen_port, fps, jpeg_quality, time_limit, http_filters, r, resize_w, resize_h, motion_compatible, allow_admin, archive_access, snapshot_dir, is_rest);
				ci -> interfaces.push_back(h);
			}
		}
		catch(const SettingNotFoundException &nfex) {
			log(LL_INFO, " no HTTP server");
		}
	//	catch(SettingException & se) {
	//		error_exit(false, "Error in %s", se.getPath());
	//	}

		//***

		log(LL_INFO, "Configuring the video-loopback...");
		try
		{
			const Setting &o_vlb = instance_root["video-loopback"];

			const std::string id = cfg_str(o_vlb, "id", "some identifier; visible in e.g. the http server", true, "");

			std::string dev = cfg_str(o_vlb, "device", "Linux v4l2 device to connect to", true, "");

			double fps = 0, interval = 0;
			if (!find_interval_or_fps(o_vlb, &interval, "fps", &fps))
				interval_fps_error("fps", "for sending frames to loopback", id.c_str());

			std::vector<filter *> *filters = NULL;
			try {
				const Setting & f = o_vlb.lookup("filters");
				filters = load_filters(f, s, r);
			}
			catch(SettingNotFoundException & snfe) {
			}

			interface *f = new v4l2_loopback(id, s, fps, dev, filters);
			ci -> interfaces.push_back(f);
		}
		catch(const SettingNotFoundException &nfex) {
			log(LL_INFO, " no video loopback");
		}

		//***

		log(LL_INFO, "Configuring the motion trigger(s)...");
		try
		{
			const Setting &o_mt = instance_root["motion-trigger"];
			size_t n_vlb = o_mt.getLength();

			log(LL_DEBUG, " %zu motion trigger(s)", n_vlb);

			if (n_vlb == 0)
				log(LL_WARNING, " 0 triggers, is that correct?");

			for(size_t i=0; i<n_vlb; i++) {
				const Setting &trigger = o_mt[i];

				const std::string id = cfg_str(trigger, "id", "some identifier; visible in e.g. the http server", true, "");

				int noise_level = cfg_int(trigger, "noise-factor", "at what difference levell is the pixel considered to be changed", true, 32);
				double pixels_changed_perctange = cfg_float(trigger, "pixels-changed-percentage", "what %% of pixels need to be changed before the motion trigger is triggered", true, 1.0);

				int min_duration = cfg_int(trigger, "min-duration", "minimum number of frames to record", true, 5);
				int mute_duration = cfg_int(trigger, "mute-duration", "how long not to record (in frames) after motion has stopped", true, 5);
				int warmup_duration = cfg_int(trigger, "warmup-duration", "how many frames to ignore so that the camera can warm-up", true, 10);
				int pre_motion_record_duration = cfg_int(trigger, "pre-motion-record-duration", "how many frames to record that happened before the motion started", true, 10);

				double max_fps = cfg_float(trigger, "max-fps", "maximum number of frames per second to analyze (or -1.0 for no limit)", true, -1.0);
				if (max_fps == 0)
					error_exit(false, "Motion triggers: max-fps must be either > 0 or -1.0. Use -1.0 for no FPS limit.");

				std::string selection_bitmap = cfg_str(trigger, "selection-bitmap", "bitmaps indicating which pixels to look at. must be same size as webcam image and must be a .pbm-file. leave empty to disable.", true, "");
				selection_mask *sm = selection_bitmap.empty() ? NULL : new selection_mask(r, selection_bitmap);

				std::vector<filter *> *filters_detection = NULL;
				try {
					const Setting & f = trigger.lookup("filters-detection");
					filters_detection = load_filters(f, s, r);
				}
				catch(SettingNotFoundException & snfe) {
				}

	/////////////
				std::vector<target *> *motion_targets = new std::vector<target *>();

				try {
					const Setting & t = trigger.lookup("targets");

					size_t n_t = t.getLength();
					log(LL_DEBUG, " %zu motion trigger target(s)", n_t);

					if (n_t == 0)
						log(LL_WARNING, " 0 trigger targets, is that correct?");

					for(size_t i=0; i<n_t; i++) {
						const Setting & ct = t[i];

						motion_targets -> push_back(load_target(ct, s, r));
					}
				}
				catch(SettingNotFoundException & snfe) {
				}
	//////////
				std::string file = cfg_str(trigger, "trigger-plugin-file", "filename of motion detection plugin", true, "");
				ext_trigger_t *et = NULL;
				if (!file.empty()) {
					et = new ext_trigger_t;
					et -> par = cfg_str(trigger, "trigger-plugin-parameter", "parameter for motion detection plugin", true, "");
					et -> library = dlopen(file.c_str(), RTLD_NOW);
					if (!et -> library)
						error_exit(true, "Failed opening motion detection plugin library %s", file.c_str());

					et -> init_motion_trigger = (init_motion_trigger_t)find_symbol(et -> library, "init_motion_trigger", "motion detection plugin", file.c_str());
					et -> detect_motion = (detect_motion_t)find_symbol(et -> library, "detect_motion", "motion detection plugin", file.c_str());
					et -> uninit_motion_trigger = (uninit_motion_trigger_t)find_symbol(et -> library, "uninit_motion_trigger", "motion detection plugin", file.c_str());
				}

				std::string exec_start = cfg_str(trigger, "exec-start", "script to start when recording starts", true, "");
				std::string exec_end = cfg_str(trigger, "exec-end", "script to start when the recording stops", true, "");

				interface *m = new motion_trigger(id, s, noise_level, pixels_changed_perctange, min_duration, mute_duration, warmup_duration, pre_motion_record_duration, filters_detection, motion_targets, sm, et, max_fps, exec_start, exec_end);
				ci -> interfaces.push_back(m);
			}
		}
		catch(const SettingNotFoundException &nfex) {
			log(LL_INFO, " no motion trigger defined");
		}

		//***
		log(LL_INFO, "Configuring the stream-to-disk backend(s)...");
		try {
			const Setting & t = instance_root.lookup("stream-to-disk");

			size_t n_t = t.getLength();
			log(LL_DEBUG, " %zu disk streams", n_t);

			if (n_t == 0)
				log(LL_WARNING, " 0 streams, is that correct?");

			for(size_t i=0; i<n_t; i++) {
				const Setting & ct = t[i];

				interface *target = load_target(ct, s, r);
				ci -> interfaces.push_back(target);
			}
		}
		catch(SettingNotFoundException & snfe) {
			log(LL_INFO, " no stream-to-disk backends defined");
		}

		if (!s)
			error_exit(false, "No video-source selected");

		if (pid_file && !do_fork)
			log(LL_WARNING, "Will not write a PID file when not forking");

		if (s)
			ci -> interfaces.push_back(s);

		cfg.instances.push_back(ci);
	}

	std::set<std::string> check_id;
	for(instance_t * inst : cfg.instances) {
		for(const interface *const i: inst -> interfaces) {
			std::string cur = i -> gen_id();

			if (check_id.find(cur) != check_id.end())
				log(LL_WARNING, "There are multiple modules/sections with the same ID (%s): this will give problems when trying to use the REST interface!", i -> get_id_str().c_str());
			else
				check_id.insert(cur);
		}
	}

	cfg.lock.unlock();

	if (do_fork) {
		if (daemon(0, 0) == -1)
			error_exit(true, "daemon() failed");

		if (pid_file) {
			FILE *fh = fopen(pid_file, "w");
			if (!fh)
				error_exit(true, "Cannot create %s", pid_file);


			fprintf(fh, "%d", getpid());
			fclose(fh);
		}

		for(;;)
			sleep(86400);
	}

	log(LL_INFO, "Starting threads");
	for(instance_t * i : cfg.instances) {
		for(interface *t : i -> interfaces)
			t -> start();
	}

	log(LL_INFO, "System started");

	getchar();

	log(LL_INFO, "Terminating");

	cfg.lock.lock();

	for(instance_t * i : cfg.instances) {
		for(interface *t : i -> interfaces)
			t -> stop();
	}

	log(LL_DEBUG, "Waiting for threads to terminate");

	for(instance_t * i : cfg.instances) {
		for(interface *t : i -> interfaces)
			delete t;

		delete i;
	}

	delete r;

// FIXME	if (et) {
// FIXME		dlclose(et -> library);
// FIXME		delete et;
// FIXME	}

	curl_global_cleanup();

	log(LL_INFO, "Bye bye");

	return 0;
}
