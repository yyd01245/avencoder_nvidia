#ifndef __AV_MUXER_INTERFACE_H
#define __AV_MUXER_INTERFACE_H

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
}InputParams_avmuxer;

typedef void ts_muxer_t;

ts_muxer_t* init_ts_muxer(InputParams_avmuxer* pInputParams);

int write_video_frame(ts_muxer_t *avmuxer,const char *video_buffer,int video_buffer_size);

int write_audio_frame(ts_muxer_t *avmuxer,const char *audio_buffer,int audio_buffer_size);

int video_pts(ts_muxer_t *avmuxer,double *videopts);

int audio_pts(ts_muxer_t *avmuxer,double *audiopts);

int uninit_ts_muxer(ts_muxer_t *avmuxer);

unsigned long get_bbcvudp_real_send_bytes(ts_muxer_t *avmuxer);


#endif
