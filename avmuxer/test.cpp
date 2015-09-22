#include <iostream>
using namespace std;
#include "CAVMuxer.h"

void usage(char** argv)
{
	printf("usage: %s <number of pulseaudio monitor> <ip:port>\n", argv[0]);
}

int main(int argc, char** argv)
{
	if (argc < 2) {
		usage(argv);
		return 0;
	}
	//string fileName = "mux-test.ts";
    string fileName = "udp://";
	fileName += string(argv[2]);
	fileName += "?pkt_size=188";
	InputParams params;
	memset(&params, 0, sizeof(InputParams));
    params.codecID = KY_CODEC_ID_MPEG2VIDEO;
    params.nBitRate = 3000;
    params.nWidth = 720;
    params.nHeight = 576;
    params.nFramerate = 25;
    params.nSamplerate = 48000;

	string monitor = argv[1];
	monitor += ".monitor";
    params.monitorName = const_cast<char*>(monitor.c_str());
	params.appName = argv[0];

    //params.bDisableFFmpeg = true;

    ts_muxer_t* muxer = create_muxer();
    init_muxer(muxer, fileName.c_str(), &params);
    for (;;) {
        // write_video_frame(m_pFormatCtx, m_pVideoStream);
        muxer_write_video(muxer, NULL, NULL);
    }

	return 0;
}
