files=videosource.cpp

libs=-lpthread

DEBUG=-g -D_DEBUG

obj:$(files)
	g++ -c $^

libvideosource.so: $(files)
	g++ -shared -fPIC $(files) $(libs) $(DEBUG) -D__linux__ -o libvideosource.so

install:obj
	cp *.o ../avencoder/
	
uninstall:
	rm -rf /usr/local/lib/libvideosource.so
	rm -rf /usr/local/include/videosource.h
	
videosource:
	-rm -f videosource
	g++ test.cpp -fPIC $(files) $(libs) $(DEBUG) -D__linux__ -o videosource
clean:
	-rm -f libvideosource
	-rm -f *.so
	-rm -f *.o

