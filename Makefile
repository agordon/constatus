# (C) 2017 by Folkert van Heusden, released under AGPL v3.0

DEBUG=-ggdb3
NAME="constatus"
PREFIX=/usr
VERSION="2.0"
CXXFLAGS=$(DEBUG) -O3 -march=native -ffast-math -D_FILE_OFFSET_BITS=64 -pedantic -std=c++11 -DNAME=\"$(NAME)\" -DVERSION=\"$(VERSION)\" -O3 `pkg-config --cflags jansson libavformat libswscale libavcodec libavutil libconfig++ cairo exiv2 sqlite3` -I/usr/include/netpbm # -march=native -mtune=native -fomit-frame-pointer -flto
LDFLAGS=$(DEBUG) -ldl -ljpeg -lpng -lcurl -lgwavi `pkg-config --libs jansson libavformat libswscale libavcodec libavutil libconfig++ cairo exiv2 sqlite3` -lswresample -lnetpbm -Wl,--export-dynamic # -flto
OBJS=source.o main.o error.o source_v4l.o utils.o picio.o filter.o filter_mirror_v.o filter_noise_neighavg.o http_client.o source_http_jpeg.o filter_mirror_h.o filter_add_text.o filter_grayscale.o exec.o filter_boost_contrast.o filter_marker_simple.o http_server.o filter_overlay.o source_stream.o filter_add_scaled_text.o filter_plugin.o log.o target.o target_avi.o target_jpeg.o target_plugin.o motion_trigger.o interface.o v4l2_loopback.o source_plugin.o cairo.o resize.o resize_cairo.o target_ffmpeg.o filter_motion_only.o target_extpipe.o meta.o filter_apply_mask.o target_vnc.o selection_mask.o resize_crop.o db.o cleaner.o source_http_mjpeg.o

constatus: $(OBJS)
	$(CXX) $(LDFLAGS) -o constatus -pthread $(OBJS)

install: constatus
	install -Dm755 constatus ${DESTDIR}${PREFIX}/bin/constatus
	install -Dm755 motion-to-constatus.py ${DESTDIR}${PREFIX}/bin/motion-to-constatus.py
	mkdir -p ${DESTDIR}${PREFIX}/share/doc/constatus
	install -Dm644 constatus.cfg ${DESTDIR}${PREFIX}/share/doc/constatus/example.cfg
	install -Dm644 man/constatus.1 ${DESTDIR}${PREFIX}/share/man/man1/constatus.1

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/constatus

clean:
	rm -f $(OBJS) constatus

package: clean
	mkdir constatus-$(VERSION)
	cp -a man/ meta-plugins filter-plugins motiontrigger-plugins source-plugins streamwriter-plugins *.cpp *.h README README.rest CHANGES constatus.cfg Makefile motion-to-constatus.py constatus-$(VERSION)
	tar czf constatus-$(VERSION).tgz constatus-$(VERSION)
	rm -rf constatus-$(VERSION)

ccheck:
	cppcheck -v --force -j 3 --enable=all --std=c++11 --inconclusive -I. . 2> err.txt
