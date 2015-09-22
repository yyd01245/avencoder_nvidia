files=audiosource.cpp

libs=-lpthread

DEBUG=-g -D_DEBUG

AR=ar 
ARFLAGS=-cru

obj:$(files)
	g++ -c $^

libaudiosource.so: $(files)
	g++ -shared -fPIC $(files) $(libs) $(DEBUG) -D__linux__ -o libaudiosource.so

install:obj
	cp *.o ../avencoder/
	
uninstall:
	rm -rf /usr/local/lib/libaudiosource.so
	rm -rf /usr/local/include/audiosource.h
	
audiosource:
	-rm -f audiosource
	g++ test.cpp -fPIC $(files) $(libs) $(DEBUG) -D__linux__ -o audiosource
clean:
	-rm -f libaudiosource
	-rm -f *.so
	-rm -f *.o

