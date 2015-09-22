
CC=gcc
NVCC=/usr/local/cuda/bin/nvcc

SRCS=tencodenv.c encodenv.c #formats.c 
#CUSRCS=rgb2nv12.cu cu_formats.cu
CUSRCS=formats.cu

OBJS=$(SRCS:.c=.o)
CUOBJS=$(CUSRCS:.cu=.o)


######################
##MACROS
######################


#-D_TEST_
# test encodeing, Send `Ctrl-C` to stop


#-DMAPPED_ONLY
# MappedResource Always set,using Resource allocate by `CUDA driver API` OR `DircetX`(Not implemented yet) .
# Remove redundant code..


#-D_DEBUG_
# Enable more output;
# Ctrl-C give profile information (e.g. Encode Speed Statistic)

LIBS=-lcuda -lnvidia-encode -lpthread -lcudart -lformats
CULIBS=-lcudart

#CFLAGS=-I /usr/local/cuda/include -L /usr/local/cuda/lib64 -g   -D_DEBUG_ -DCUDA_CONVERT -DMAPPED_ONLY #-D_TEST_
CFLAGS=-I /usr/local/cuda/include -L /usr/local/cuda/lib64 -L. -O2 -fPIC  -DCUDA_CONVERT -DMAPPED_ONLY
CUFLAGS=-I /usr/local/cuda/include -L /usr/local/cuda/lib64 -O2 

DATE=$(shell date +%m-%d)




.PHONY:all codecnv encodenv decodenv digest clean cleanall install


all: tdec
#encodenv

tdec:decodenv.c
	gcc decodenv.c -I /usr/local/cuda/include/  -L /usr/local/cuda/lib -lnvcuvid -lcuda -o decodenv -lbsd


codecnv:libencodenv.so libdecodenv.so

encodenv:libencodenv.so

decodenv:libdecodenv.so


libdecodenv.so:decodenv.c
	${CC} -o $@ -fPIC -shared $^


libencodenv.so:$(filter-out t%.c,${SRCS}) libformats.so 
	${CC} -fPIC -shared -o $@ ${CFLAGS}  $^ ${LIBS} 



tencodenv:tencodenv.c
	${CC} -o $@ -Wl,--rpath=./ -L. $^ -lformats -lencodenv


#gcc decodenv.c -I /usr/local/cuda/include/  -L /usr/local/cuda/lib -lnvcuvid -lcuda -o decodenv


%.o:%.c
	${CC} -c -o $@ ${CFLAGS}  $< ${LIBS}


%.o:%.cu
	${NVCC} -c -o $@ ${CUFLAGS}  $< ${CULIBS}

lib%.a:%.cu
	${NVCC} -lib -o $@ ${CUFLAGS}  $< ${CULIBS}

lib%.so:%.cu
	${NVCC} -shared -Xcompiler -fPIC -o $@ ${CUFLAGS}  $< ${CULIBS}

tar:
	tar -jcf ../NvEncodec-${DATE}.tar.bz2 ../NvEncodec/


install:
	sudo cp *.so /usr/local/lib


clean:
	-rm *.o *.a *.so
	-rm tencodenv

cleanall: clean
	-rm *.264





