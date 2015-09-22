files=tssmooth.cpp

libs=-lpthread

DEBUG=-g -D_DEBUG

libtssmooth.so: $(files)
	g++ -shared -fPIC $(files) $(libs) $(DEBUG) -D__linux__ -o libtssmooth.so

install:libtssmooth.so
	-strip -x libtssmooth.so
	cp libtssmooth.so /usr/local/lib/
	cp tssmooth.h /usr/local/include/
	
uninstall:
	rm -rf /usr/local/lib/libtssmooth.so
	rm -rf /usr/local/include/tssmooth.h
	
tssmooth:
	-rm -f tssmooth
	g++ test.cpp -fPIC $(files) $(libs) $(DEBUG) -D__linux__ -o tssmooth
clean:
	-rm -f libtssmooth
	-rm -f *.so

