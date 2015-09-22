#!/bin/bash

COMPILW_PATH=`pwd`

cd  ${COMPILW_PATH}/audioencoder/

make clean;make;sudo make install;

cd  ${COMPILW_PATH}/audiosource/

make clean;make;sudo make install;

cd  ${COMPILW_PATH}/avmuxer/

make clean;make;sudo make install;

cd  ${COMPILW_PATH}/tssmooth/

make clean;make;sudo make install;

cd  ${COMPILW_PATH}/videosource_bvbcs/

make clean;make;sudo make install;

cd  ${COMPILW_PATH}/videoencoder_nvidia/

make clean;make;sudo make install;

cd  ${COMPILW_PATH}/avencoder/

make clean;make;

sudo cp avencoder /usr/local/bin
