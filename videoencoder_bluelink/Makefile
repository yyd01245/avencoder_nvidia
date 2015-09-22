files=videoencoder.cpp

libs=-ldm816x -lavutil -lswscale -lavcodec -lippccem64t

DEBUG=-g -D_DEBUG

obj:$(files)
	g++ -c $^

libvideoencoder.so: $(files)
	g++ -shared -fPIC $(files) $(libs) $(DEBUG) -D__linux__ -o libvideoencoder.so

install:obj
	cp *.o ../avencoder/
	
uninstall:
	rm -rf /usr/local/lib/libvideoencoder.so
	rm -rf /usr/local/include/videoencoder.h
	
videoencoder:
	-rm -f videoencoder
	g++ test.cpp -fPIC $(files) $(libs) $(DEBUG) -D__linux__ -o videoencoder
clean:
	-rm -f libvideoencoder
	-rm -f *.so
	-rm -f *.o

