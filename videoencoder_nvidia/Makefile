files=videoencoder.cpp 

encnvdir=encodec_nv
encnvso=${encnvdir}/libencodenv.so


LIBS=-lpthread -ldl -lippccem64t 

DEBUG=-g -D_DEBUG 

DATE=$(shell date +%Y%m%d)


CWD=$(shell pwd)
DIR=$(notdir ${CWD})
TARNAME=$(notdir $(shell pwd))-${DATE}.tar.gz


obj:$(files)                                                                                                        
	g++ -c $^

#.PHONY:all install uninstall clean tar


all:libvideoencoder.so ${encnvso}
	@echo Make "<"$^">"
#	@echo ${TARNAME}     ${DIR}

${encnvso}:
	(cd ${encnvdir} && make && sudo make install)



libvideoencoder.so: $(files) 
	g++ -shared -fPIC $< ${LIBS} ${DEBUG} -o libvideoencoder.so

install:obj
	cp *.o ../avencoder/
#	-strip -x libvideoencoder.so
	
uninstall:
	rm -rf /usr/local/lib/libvideoencoder.so
	rm -rf /usr/local/lib/libencodenv.so
	rm -rf /usr/local/lib/libformats.so
	rm -rf /usr/local/include/videoencoder.h
	
tar:
	@echo TAR ${TARNAME}
	tar cf ../${TARNAME} ../${DIR}


clean:
	-rm -f *.o

