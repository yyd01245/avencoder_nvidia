files=videoencoder.cpp relnv.c

#libs=-lpthread -ldl -L /opt/intel/ipp/6.1.6.063/em64t/sharedlib/  -I /opt/intel/ipp/6.1.6.063/em64t/include -lippccem64t -lnvidia-ml
libs=-lpthread -ldl  -lnvidia-ml

DEBUG=-g -D_DEBUG

libvideoencoder.so: $(files)
	cp nvml.h /usr/local/include
	g++ -shared -fPIC $(files) $(libs) $(DEBUG) -D__linux__ -o libvideoencoder.so

install:libvideoencoder.so
	-strip -x libvideoencoder.so
	cp libvideoencoder.so /usr/local/lib/
	cp videoencoder.h /usr/local/include/
	cp ASVEncoder_interface.h /usr/local/include/
	cp nvml.h /usr/local/include
	
uninstall:
	rm -rf /usr/local/lib/libvideoencoder.so
	rm -rf /usr/local/include/videoencoder.h
	
videoencoder:
	-rm -f videoencoder
	g++ test.cpp -fPIC $(files) $(libs) $(DEBUG) -D__linux__ -o videoencoder
clean:
	-rm -f videoencoder
	-rm -f *.so

