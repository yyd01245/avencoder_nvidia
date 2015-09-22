files=videoencoder.cpp

libs=-lpthread -lvaenc -L /opt/intel/ipp/6.1.6.063/em64t/sharedlib/ -I /opt/intel/ipp/6.1.6.063/em64t/include -lippccem64t -lmfxhw64

DEBUG=-g -D_DEBUG

libvideoencoder.so: $(files)
	g++ -shared -fPIC $(files) $(libs) $(DEBUG) -D__linux__ -o libvideoencoder.so

install:libvideoencoder.so
	-strip -x libvideoencoder.so
	cp libvideoencoder.so /usr/local/lib/
	cp videoencoder.h /usr/local/include/
	
	
	cp buffdef.h /usr/local/include/
	cp msdk_encode.h /usr/local/include/
	cp mfxvideo.h /usr/local/include/
	cp mfxsession.h /usr/local/include/
	cp mfxcommon.h /usr/local/include/
	cp mfxdefs.h /usr/local/include/
	cp mfxstructures.h /usr/local/include/
	
uninstall:
	rm -rf /usr/local/lib/libvideoencoder.so
	rm -rf /usr/local/include/videoencoder.h
	
videoencoder:
	-rm -f videoencoder
	g++ test.cpp -fPIC $(files) $(libs) $(DEBUG) -D__linux__ -o videoencoder
clean:
	-rm -f libvideoencoder
	-rm -f *.so

