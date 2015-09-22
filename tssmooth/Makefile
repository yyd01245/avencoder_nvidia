files=tssmooth.cpp

libs=-lpthread

DEBUG=-g -D_DEBUG

obj:$(files)
	g++ -c $^


libtssmooth.so: $(files)
	g++ -shared -fPIC $(files) $(libs) $(DEBUG) -D__linux__ -o libtssmooth.so

install:obj
	cp *.o ../avencoder/
	
uninstall:
	rm -rf /usr/local/lib/libtssmooth.so
	rm -rf /usr/local/include/tssmooth.h
	
tssmooth:
	-rm -f tssmooth
	g++ test.cpp -fPIC $(files) $(libs) $(DEBUG) -D__linux__ -o tssmooth
clean:
	-rm -f libtssmooth
	-rm -f *.so
	-rm -f *.o

