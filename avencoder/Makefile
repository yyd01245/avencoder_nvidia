files=avencoder.cpp 

libs=-pthread -lavformat -lavcodec -lavutil -lswscale -lippccem64t -ldl

DEBUG=-g -D_DEBUG

TARGET=avencoder

all:$(TARGET)
	
obj:$(files)
	g++ -c $^

$(TARGET):avencoder.o audioencoder.o audiosource.o videoencoder.o videosource.o tssmooth.o avmuxer.o CAVMuxer.o
	g++ $^ -fPIC $(libs) $(DEBUG) -D__linux__ -o $@

avencoder_old:avencoder.cpp
	-rm -f avencoder
	g++ $(files) -fPIC $(libs) $(DEBUG) -D__linux__ -o avencoder
clean:
	-rm -f avencoder

