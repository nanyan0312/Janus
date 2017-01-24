CC=gcc
CXX=g++
CXXFLAGS=-Wall -g
CPPFLAGS=-DENABLE_LOGGING
INCPATH=-I.  -I./include
LIBS=-L./lib -lhiredis
OBJS=janus_lock.o janus_redis_cluster.o janus_locker.o janus_utils.o demo.o
LIBOBJS=$(filter-out demo.o, $(OBJS))
INCFILES=janus_lock.h janus_redis_cluster.h janus_locker.h

.PHONY: all clean

all: demo libjanus.a
	@echo "make all done"

clean:
	rm -rf demo *.o *.a
	rm -rf output

demo: $(OBJS)
	$(CXX) $(OBJS) $(LIBS) -o demo
	mkdir -p output/bin
	cp -f --link demo output/bin
	rm -r demo

libjanus.a:
	ar crs libjanus.a $(LIBOBJS)
	mkdir -p output/lib
	mkdir -p output/include
	cp -f --link libjanus.a output/lib
	cp -f --link $(INCFILES) output/include
	rm -r libjanus.a

janus_lock.o: janus_lock.cpp janus_lock.h
	$(CXX) $(INCPATH) $(CPPFLAGS) $(CXXFLAGS) -c janus_lock.cpp

janus_redis_cluster.o: janus_redis_cluster.cpp janus_redis_cluster.h janus_globals.h janus_builtin_lua.h
	$(CXX) $(INCPATH) $(CPPFLAGS) $(CXXFLAGS) -c janus_redis_cluster.cpp

janus_locker.o: janus_locker.cpp janus_locker.h janus_lock.h janus_redis_cluster.h janus_globals.h janus_utils.h
	$(CXX) $(INCPATH) $(CPPFLAGS) $(CXXFLAGS) -c janus_locker.cpp

janus_utils.o: janus_utils.cpp janus_utils.h
	$(CXX) $(INCPATH) $(CPPFLAGS) $(CXXFLAGS) -c janus_utils.cpp

demo.o: demo.cpp
	$(CXX) $(INCPATH) $(CPPFLAGS) $(CXXFLAGS) -c demo.cpp
