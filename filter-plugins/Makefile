# (C) 2017 by folkert van heusden, this file is released in the public domain
INSTALL_PREFIX=/usr/local

CFLAGS=-fPIC -Wall -pedantic -ggdb3 `pkg-config --cflags opencv`
CXXFLAGS=-fPIC -Wall -pedantic -ggdb3 `pkg-config --cflags opencv`
VERSION=0.1

OBJS1=demo.o
OBJS2=demo2.o
OBJS3=demoocv.o
OBJS4=demoocv2.o

all: lib1 lib2 lib3 lib4

lib1: $(OBJS1)
	ar -r demo.a $(OBJS1)
	ld -g -x -lm -shared -soname demo.so.2 -o demo.so.$(VERSION) --whole-archive demo.a

lib2: $(OBJS2)
	ar -r demo2.a $(OBJS2)
	ld -g -x -lm -shared -soname demo2.so.2 -o demo2.so.$(VERSION) --whole-archive demo2.a

lib3: $(OBJS3)
	ar -r demoocv.a $(OBJS3)
	ld -g -x -lm `pkg-config --libs opencv` -shared -soname demoocv.so.2 -o demoocv.so.$(VERSION) --whole-archive demoocv.a

lib4: $(OBJS4)
	ar -r demoocv2.a $(OBJS4)
	ld -g -x -lm `pkg-config --libs opencv` -shared -soname demoocv2.so.2 -o demoocv2.so.$(VERSION) --whole-archive demoocv2.a 

install: all
	cp demo.so.* demo2.so.* demoocv.so.* $(INSTALL_PREFIX)/lib
	/sbin/ldconfig

uninstall: clean
	rm -f $(INSTALL_PREFIX)/lib/demo.so.* $(INSTALL_PREFIX)/lib/demo2.so.* $(INSTALL_PREFIX)/lib/demoocv.so.*

clean:
	rm -f $(OBJS1) $(OBJS2) demo.a demo.so.* core demo2.a demo2.so.* $(OBJS3) demoocv.so.* demoocv.a
