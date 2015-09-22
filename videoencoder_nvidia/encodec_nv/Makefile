


.PHONY:all clean



CFLAGS= -I/usr/local/cuda/include 

LIBS=-lcuda -lnvidia-ml 

SRCS=NvEncoder_bbcv.cpp NvHWEncoder.cpp confile.c



all:libnvencoder


clean:
	-rm *.so
	-rm *.o

libnvencoder:libnvencoder.so

install:
	cp libnvencoder.so /usr/local/lib/
	ldconfig

libnvencoder.so:${SRCS}
	${CXX} -fPIC -shared  ${CFLAGS} $^ -o $@ ${LIBS}



