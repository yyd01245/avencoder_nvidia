


VIDENC=videoencoder_nvidia


.PHONY :all



all:
	-mkdir lib bin
	(cd videosource && make)
	cp videosource/*.so lib
	(cd audiosource && make)
	cp audiosource/*.so lib
	(cd audioencoder&& make)
	cp audioencoder/*.so lib
	(cd avmuxer && make)
	cp avmuxer/*.so lib
	(cd tssmooth && make)
	cp tssmooth/*.so lib
	(cd avencoder && make)
	cp avencoder/avencoder bin
	(cd ${VIDENC} && make)
	cp ${VIDENC}/*.so lib
	cp ${VIDENC}/encodec_nv/*.so lib
