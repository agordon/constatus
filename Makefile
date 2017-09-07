# (C) 2017 by Folkert van Heusden, released under AGPL v3.0

DEBUG=-ggdb3
NAME="constatus"
PREFIX=/usr/local
VERSION="1.5"
CXXFLAGS=$(DEBUG) -pedantic -std=c++11 -DNAME=\"$(NAME)\" -DVERSION=\"$(VERSION)\" -O3 # -march=native -mtune=native -fomit-frame-pointer -flto
LDFLAGS=$(DEBUG) -ldl -ljpeg -lpng -lcurl -ljansson -lgwavi `pkg-config --cflags --libs libavformat libswscale libavcodec libavutil libavresample` -lswresample `pkg-config --libs cairo` -lnetpbm # -flto
OBJS=source.o main.o error.o source_v4l.o utils.o picio.o filter.o filter_mirror_v.o filter_noise_neighavg.o http_client.o source_http_jpeg.o filter_mirror_h.o filter_add_text.o source_http_mjpeg.o filter_grayscale.o exec.o filter_boost_contrast.o filter_marker_simple.o http_server.o filter_overlay.o source_rtsp.o filter_add_scaled_text.o filter_plugin.o log.o target.o target_avi.o target_jpeg.o target_plugin.o motion_trigger.o interface.o v4l2_loopback.o source_plugin.o cairo.o resize.o resize_cairo.o target_ffmpeg.o

constatus: $(OBJS)
	$(CXX) $(LDFLAGS) -o constatus -pthread $(OBJS)

install: constatus
	install -Dm755 constatus ${DESTDIR}${PREFIX}/bin/constatus
	mkdir -p ${DESTDIR}${PREFIX}/share/doc/constatus
	install -Dm644 constatus-v4l.conf ${DESTDIR}${PREFIX}/share/doc/constatus/constatus-v4l.conf
	install -Dm644 constatus-v4l-loopback.conf ${DESTDIR}${PREFIX}/share/doc/constatus/constatus-v4l-loopback.conf
	install -Dm644 constatus-rtsp.conf ${DESTDIR}${PREFIX}/share/doc/constatus/constatus-rtsp.conf
	install -Dm644 constatus-jpeg.conf ${DESTDIR}${PREFIX}/share/doc/constatus/constatus-jpeg.conf
	install -Dm644 constatus-mjpeg.conf ${DESTDIR}${PREFIX}/share/doc/constatus/constatus-mjpeg.conf

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/constatus

clean:
	rm -f $(OBJS) constatus

package: clean
	mkdir constatus-$(VERSION)
	cp -a *.cpp *.h constatus*conf Makefile constatus-$(VERSION)
	tar czf constatus-$(VERSION).tgz constatus-$(VERSION)
	rm -rf constatus-$(VERSION)

check:
	cppcheck -v --force -j 3 --enable=all --std=c++11 --inconclusive -I. . 2> err.txt
