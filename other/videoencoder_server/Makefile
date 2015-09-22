files=main.cpp videoencoder.cpp

libs=-lm -ldm816x -ldl -lpthread -lavutil

DEBUG=-g -D_DEBUG

videoencoder: $(files)
	g++  $(files) -fPIC $(libs) $(DEBUG) -D__linux__ -o videoencoder

clean:
	-rm -f videoencoder

