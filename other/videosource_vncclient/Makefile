files=videosource.cpp

libs=-lpthread -lvncclient

DEBUG=-g -D_DEBUG

libvideosource.so: $(files)
	g++ -shared -fPIC $(files) $(libs) $(DEBUG) -D__linux__ -o libvideosource.so

install:libvideosource.so
	-strip -x libvideosource.so
	cp libvideosource.so /usr/local/lib/
	cp videosource.h /usr/local/include/
	
uninstall:
	rm -rf /usr/local/lib/libvideosource.so
	rm -rf /usr/local/include/videosource.h
	
videosource:
	-rm -f videosource
	g++ test.cpp -fPIC $(files) $(libs) $(DEBUG) -D__linux__ -o videosource
clean:
	-rm -f libvideosource
	-rm -f *.so

