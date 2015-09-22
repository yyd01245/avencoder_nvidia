files=videoencoder.cpp

libs=-lippccem64t

DEBUG=-g -D_DEBUG

libvideoencoder.so: $(files)
	g++ -shared -fPIC $(files) $(libs) $(DEBUG) -D__linux__ -o libvideoencoder.so

install:libvideoencoder.so
	-strip -x libvideoencoder.so
	cp libvideoencoder.so /usr/local/lib/
	cp videoencoder.h /usr/local/include/
	
uninstall:
	rm -rf /usr/local/lib/libvideoencoder.so
	rm -rf /usr/local/include/videoencoder.h
	
videoencoder:
	-rm -f videoencoder
	g++ test.cpp -fPIC $(files) $(libs) $(DEBUG) -D__linux__ -o videoencoder
clean:
	-rm -f libvideoencoder
	-rm -f *.so

