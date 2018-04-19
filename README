Records a videofile when motion is detected via a camera.
This camera can be locally attached to your Linux system or be accessed via MJPEG.

Supports:
  input:
   - video4linux
   - MJPEG cameras
   - RTSP cameras
   - JPEG cameras
   - plugin interface (included is a VNC client plugin)

  output:
   - AVI file(s)
   - individual JPEGs
   - HTTP streaming
   - video4linux loopback device
   - build-in VNC server
   - via plugins

  motion detection
   - configurable detector
   - masks
   - also via plugin so that you can easily use one from e.g. OpenCV

  filters:
   - add generic text
     two versions: fast (low CPU) or scaled (text can be any font, size or color)
   - boost contrast
   - convert to grayscale
   - add box around movement
   - show only pixels that have changed
   - mirror vertical and/or horizontal
   - neighbour average noise filter
   - picture overlay (with alpha blending)
   - scaling (resizing)
   - filters plugins (.so-files that can be linked at runtime)

  managing:
   - Web interface
   - REST interface

Required:
- libcurl-dev e.g. libcurl4-openssl-dev
- libjansson-dev
- libconfig++-dev
- libjpeg-dev
- libpng-dev
- libgwavi - from https://github.com/Rolinh/libgwavi.git
- libavformat-dev / libswscale-dev / libavcodec-dev / libavutil-dev / libswresample-dev / libavresample-dev
- libcairo2-dev
- libnetpbm10-dev
- libexiv2-dev
- libsqlite3-dev

If you don't want to use libexiv2 or ligwavi, then remove them from the Makefile and config.h.


The program supports cameras that generate YUYV, JPEG, RGB or YUV420 output.
If your camera emits something different, e.g. P207 (PAC207) then you can use libv4l (by Hans de Goede).
E.g.:
LD_PRELOAD=/usr/lib/arm-linux-gnueabihf/libv4l/v4l2convert.so ./constatus -c constatus-v4l.conf (on the raspberry pi)
or
LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libv4lconvert.so ./constatus -c constatus-v4l.conf         (on x64)


The program is configured via a text-file. It is processed by libconfig and thus uses that format.
Take a look at the example: constatus.cfg.
Use motion-to-constatus.py to convert a motion configuration file to a constatus configuration file - this requires the libconf package, see https://pypi.python.org/pypi/libconf

Each configuration has 1 or more instances.

Each instance has 1 (video-)source. So if you want to monitor multiple cameras, then you need to start multiple instances of constatus. The idea is that if anything crashes, you won't loose all cameras.

A configuration can have 0 or more motion-triggers (e.g. to have seperate output streams for different parts of the input) and 0 or more stream-to-file backends.

Most outputs (http, files, etc) and the motion-trigger can have 0 or more filters. Filters are executed in the order in which they are found on the configuration-file. You can also have multiple instances of the same filter for an output/motion-trigger.

As said, constatus can have multiple streamers, filters, etc.; the only drawback is that it uses more cpu. Having available multiple cpus/cores/threads is an advantag for constatus.

Each item has an id. You can leave it empty if you like. It is used in the configuration web-interface as a hint to which item you're working on.

If it is unclear what the "selection-bitmap" masks, then try adding it to an "apply-mask" stream-writer or http-server filter. That way you can see which part of the image is masked off.


Please check the file CHANGES to see what you need to know when upgrading from an earlier version.
Read README.rest to get to know how the REST interface works.


(C) 2017-2018 by Folkert van Heusden, released under AGPL v3.0
mail@vanheusden.com
