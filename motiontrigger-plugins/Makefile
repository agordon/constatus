# (C) 2017 by folkert van heusden, this file is released in the public domain
INSTALL_PREFIX=/usr/local

CXXFLAGS=-fPIC -Wall -pedantic -ggdb3 `pkg-config --cflags opencv` -std=c++11
VERSION=0.1

OBJS1=demo.o
OBJS2=demoocv.o
OBJS3=demoocv2.o

all: lib1 lib2 lib3

lib1: $(OBJS1)
	ar -r demo.a $(OBJS1)
	ld -g -x -lm -shared -soname demo.so.2 -o demo.so.$(VERSION) --whole-archive demo.a

lib2: $(OBJS2)
	ar -r demoocv.a $(OBJS2)
	ld -g -x -lm `pkg-config --libs opencv` -shared -soname demoocv.so.2 -o demoocv.so.$(VERSION) --whole-archive demoocv.a

lib3: $(OBJS3)
	ar -r demoocv2.a $(OBJS3)
	ld -g -x -lm `pkg-config --libs opencv` -shared -soname demoocv2.so.2 -o demoocv2.so.$(VERSION) --whole-archive demoocv2.a 

install: all
	cp demo*.so.* $(INSTALL_PREFIX)/lib
	/sbin/ldconfig

uninstall: clean
	rm -f $(INSTALL_PREFIX)/lib/demo*.so.*

clean:
	rm -f $(OBJS1) core $(OBJS2) $(OBJS3) *.a *.so.*
