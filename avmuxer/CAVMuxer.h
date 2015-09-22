#ifndef __AV_MUXER_H
#define __AV_MUXER_H

#ifndef INT64_C
#define INT64_C(c) c##LL
#endif
#ifndef UINT64_C
#define UINT64_C(c) c##LL
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libswscale/swscale.h"

#ifdef __cplusplus
} // extern "C"
#endif

#include <queue>

typedef int KYCodecID;
#define KY_CODEC_ID_H264 			1
#define KY_CODEC_ID_MPEG2VIDEO	2

typedef struct
{
	KYCodecID codecID;
	unsigned int nWidth;
	unsigned int nHeight;
	unsigned int nBitRate;   /*b/s*/
	unsigned int nSamplerate;  /*hz*/
	unsigned int nFramerate;	/*record audio*/
	char monitorName[128];
	char appName[128];  /*当前进程名称，即argv[0]*/

	char destip[32];
	int destport;
	
	int index;
	unsigned int nPeakBitRate; // b/s

	int pmt_pid;
	int service_id;
	int video_pid;
	int audio_pid;
}InputParams;


class CAVMuxer
{
public:
    CAVMuxer(void);
    ~CAVMuxer(void);
public:

	int Init(InputParams* pInputParam);

	int WriteVideoES(uint8_t* buff, uint32_t size);

	int WriteAudioFrame(char* buff, int size);
	int WriteVideoFrame(char* buff, int size);

	double GetAudioPts();
	double GetVideoPts();

	unsigned long GetSendBytes();

	void Close();
	
private:
    bool AddVideoStream();
    bool AddAudioStream();
	
private:
    AVOutputFormat *m_pOutputFmt;
    AVFormatContext *m_pFormatCtx;
    AVStream *m_pVideoStream;
    AVStream *m_pAudioStream;
    InputParams* m_pParams; 

	uint64_t m_nVideoFrameCount;
};

#endif
