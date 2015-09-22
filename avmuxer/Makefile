files=CAVMuxer.cpp avmuxer.cpp

libs=-lavformat -lavcodec -lavutil -lswscale

DEBUG=-g -D_DEBUG

obj:$(files)
	g++ -c $^

libavmuxer.so: $(files) Makefile
	g++ -shared -fPIC $(files) $(libs) $(DEBUG) -D__linux__ -o libavmuxer.so

install:obj
	cp *.o ../avencoder/
	
uninstall:
	rm -rf /usr/local/lib/libavmuxer.so
	rm -rf /usr/local/include/avmuxer.h
	
avmuxer:
	-rm -f avmuxer
	g++ test.cpp -fPIC $(include) $(files) $(libs) $(DEBUG) -D__linux__ -o avmuxer
clean:
	-rm -f avmuxer
	-rm -f *.so
	-rm -f *.o

