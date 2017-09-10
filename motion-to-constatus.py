#! /usr/bin/python

import json
import sys

def load_file(f, d):
	includes = []

	fh = open(f, 'r')

	for line in fh.readlines():
		line = line.rstrip('\n')

		if line[0:1] == '#' or line[0:1] == ';' or len(line) == 0:
			continue

		parts = line.split(' ')

		par = ''
		for p in parts[1:]:
			if par != '':
				par += ' '

			par += p

		if parts[0] == 'camera':
			includes.append(par)
		else:
			d[parts[0]] = par

	fh.close()

	return includes

def lu(d, k, df=None):
	if k in d:
		return d[k]

	return df

def lui(d, k, df=None):
	return int(lu(d, k, df))

def luf(d, k, df=None):
	return float(lu(d, k, df))

def process(f, cfg):
	print 'processing %s' %f
	includes = load_file(f, cfg)

	j = dict()
	j['general'] = {	'logfile' : lu(cfg, 'logfile', 'constatus.log'),
                	"log-level" : "debug",
			"resize-type" : "regular"
			}

	dev = lu(cfg, 'videodevice')
	url = lu(cfg, 'netcam_url')

	fps = luf(cfg, 'framerate')
	if fps == None or fps == '100':
		fps = -1.0
	else:
		fps = float(fps)

	quality = lui(cfg, 'quality')

	td = lu(cfg, 'target_dir', './')

	width = lui(cfg, 'width', 320)
	height = lui(cfg, 'height', 240)

	if dev and url == None: # v4l
		print 'v4l'

		j['source'] = {
			"type" : "v4l",
			"device" : dev,
			"width" : width,
			"height" : height,
			"max-fps" : fps,
			"resize-width" : -1,
			"resize-height" : -1,
			"prefer-jpeg" : False,
			"rpi-workaround" : lu(cfg, 'mmalcam_name') != None, # not sure
			"quality" : quality
			}

	else: # most likely a netcam
		print 'netcam'

		if url[0:4] == 'rtsp':
			t = 'rtsp'
		else:
			t = 'mjpeg'

		j['source'] = {
			"id" : f,
			"type" : t,
			"url" : url,
			"max-fps" : fps,
			"resize-width" : -1,
			"resize-height" : -1
			}

	s_adapter = '::FFFF:127.0.0.1'
	if lu(cfg, 'stream_localhost') != 'on':
		s_adapter = '0.0.0.0'

	h = []
	h.append( {
                        "listen-adapter" : s_adapter,
                        "listen-port" : lui(cfg, 'stream_port'),
                        "fps" : luf(cfg, 'stream_maxrate', 1.0),
                        "quality" : lui(cfg, 'stream_quality'),
                        "time-limit" : lui(cfg, 'stream_limit', -1),
                        "resize-width" : -1,
                        "resize-height" : -1,
                        "motion-compatible" : True,
                        "allow-admin" : False,
			"archive-access" : False,
                        "snapshot-dir" : td,
		} )

	if lui(cfg, 'webcontrol_port', 0) != 0:
		wc_adapter = '::FFFF:127.0.0.1'
		if lu(cfg, 'webcontrol_localhost') != 'on':
			wc_adapter = '0.0.0.0'

		h.append( {
				"listen-adapter" : wc_adapter,
				"listen-port" : lui(cfg, 'webcontrol_port'),
				"fps" : luf(cfg, 'stream_maxrate', 1.0),
				"quality" : lui(cfg, 'stream_quality', quality),
				"time-limit" : lui(cfg, 'stream_limit', -1),
				"resize-width" : -1,
				"resize-height" : -1,
				"motion-compatible" : False,
				"allow-admin" : True,
				"archive-access" : True,
				"snapshot-dir" : td,
			}	 )

	j['http-server'] = h

	threshold = luf(cfg, "threshold", 1500.0)
	pcp = threshold * 100.0 / (width * height)


	target = {
			"id" : "motion output",
			"path" : td,
			"prefix" : "m-",
			"quality": quality,
			"restart-interval" : 86400,
			"fps" : fps,
			"override-fps" : -1.0,
			"stream-writer-plugin-file" : "",
			"stream-writer-plugin-parameter" : "",
			"exec-start" : lu(cfg, "on_movie_start", ""),
			"exec-cycle" : "",
			"exec-end" : lu(cfg, "on_movie_end", ""),
			"filters" : [   
				]
		}

	ep = lu(cfg, "extpipe", None)
	if ep == None:
		target["format"] = "ffmpeg"
		target["ffmpeg-type"] = "avi"
		target["ffmpeg-parameters"] = ""
		target["bitrate"] = 400000

	else:
		target["format"] = "extpipe"
		target["cmd"] = ep

	m = []
	m.append( {
                        "noise-factor" : lui(cfg, "noise_level", 32),
                        "pixels-changed-percentage" : pcp,
                        "min-duration" : lui(cfg, "post_capture", 0),
                        "mute-duration" : lui(cfg, "event_gap", 0),
                        "trigger-plugin-file" : "",
                        "trigger-plugin-parameter" : "",
                        "warmup-duration" : 10,
                        "selection-bitmap" : "",
                        "pre-motion-record-duration" : lui(cfg, "pre_capture", 0),
                        "max-fps" : fps,
                        "filters-detection" : [ ],
                        "targets" : [ target ]

		} )
	j['motion-trigger'] = m

	fh = open(f + '.c.conf', 'w')
	fh.write(json.dumps(j, sort_keys=True, indent=4, separators=(',', ': ')))
	fh.close()

	for i in includes:
		process(i, cfg)

cfg = dict()
process(sys.argv[1], cfg)
