files=audioencoder.cpp

libs=-lpthread -lavformat -lavcodec -lavutil -lswscale

DEBUG=-g -D_DEBUG
 
obj:$(files)
	g++ -c $(files) 

libaudioencoder.so: $(files)
	g++ -shared -fPIC $(files) $(libs) $(DEBUG) -D__linux__ -o libaudioencoder.so

install:obj
	cp *.o ../avencoder/

uninstall:
	rm -rf /usr/local/lib/libaudioencoder.so
	rm -rf /usr/local/include/audioencoder.h
	
audioencoder:
	-rm -f audioencoder
	g++ test.cpp -fPIC $(files) $(libs) $(DEBUG) -D__linux__ -o audioencoder
clean:
	-rm -f libaudioencoder
	-rm -f *.so
	-rm -f *.o

