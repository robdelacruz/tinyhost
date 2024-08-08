CC=gcc
CXX=g++

CSOURCES=t.c clib.c cnet.c msg.c
CPPSOURCES=
COBJECTS=$(patsubst %.c, %.o, $(CSOURCES))
CPPOBJECTS=$(patsubst %.cpp, %.o, $(CPPSOURCES))
OBJECTS=$(COBJECTS) $(CPPOBJECTS)

WX_CXXFLAGS=`wx-config --cxxflags`
WX_LIBS=`wx-config --libs std,propgrid`

CFLAGS=-g -Wall -Werror -std=gnu99 -Wno-unused
CPPFLAGS=-g -Wall -Werror
CPPFLAGS+= $(WX_CXXFLAGS)
#LDFLAGS=$(WX_LIBS)
LDFLAGS=

#.SILENT:
all: t

dep:
	echo no deps

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

%.o: %.cpp
	$(CXX) -c $(CPPFLAGS) -o $@ $<

t: $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

tclient: tclient.c clib.c cnet.c msg.c
	$(CC) -o tclient $^ $(CFLAGS) $(LDFLAGS)

tinytest: tinytest.c clib.c cnet.c msg.c
	$(CC) -o tinytest $^ $(CFLAGS) $(LDFLAGS)

clean:
	rm -rf t tclient tinytest *.o

