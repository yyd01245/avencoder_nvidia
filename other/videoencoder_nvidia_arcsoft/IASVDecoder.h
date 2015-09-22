#ifndef _IASVDECODER_H_
#define _IASVDECODER_H_

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(__GNUC__)
#define ASVAPI __attribute__((visibility("default")))
#elif defined(_MSC_VER)
#define ASVAPI __declspec(dllexport)
#endif

#define CODEC_H264   1
#define CODEC_MPEG2  2

#define AS_VIDEO_FLAG_IS_FRAME         0x00000001 //entire frame, interlace or progressive
#define AS_VIDEO_FLAG_IS_PROGRESSIVE   0x00000002 //progressive
#define AS_VIDEO_FLAG_FIELD1FIRST      0x00000004 //interlace, top field first or bottom field first if not set.

/*
 *	Notify Control, Set Call back function to get VideoInfo & Display frame.
 */
enum ASVid_NotifyCtrl
{
	ASVid_VideoInfo,
	ASVid_DisPlay,
};

/*
 *  Video information
 */
typedef struct _ASVid_VideoInfo
{
	int  dwImgWidth;
	int  dwImgHeight;
	int  dwSARWdith;
	int  dwSARHeight;
	bool bInterlace; 
}ASVidInfo;

typedef struct _llPTS
{
	unsigned int nHigh;
	unsigned int nLow;
}llPTS;

/**
 *decoder output information
 */
typedef struct _ASVid_Output
{
    int bEOS; /* the last frame output */
    int nPicType; /* frame's type, Interlace, Progressive */
    unsigned char *pBaseView[3]; /* decoded frame data */
    int nBaseStride; /* stride of base view */
	int nCSConv;     /* color space, 0->NV12, 1->YV12. 2->I420 */

	llPTS rtStart; /* frame's PTS */
	llPTS rtStop; /* frame's stop PTS */

    struct
    {
        int nARX;
        int nARY;
    } fAspectRatio; /* frame display aspect ratio */
    int nWidth; /* frame image's width */
    int nHeight; /* frame image's height */
	void *pDevicePtr; /* unused */
    void *pReserved[64];
} ASVid_Output, *PASVid_Output;

/**
 *  video decoder notify callback function
 *  pCaller : current object.
 *  nType : 0->ASVid_VideoInfo, 1->ASVid_DisPlay
 *  pGet : ASVidInfo or ASVid_Output that get from decoder core.
 */
typedef int (*PASVidNotify)(void *pCaller, int nType, void *pGet);

/*
 *	Call back function struct.
 */
typedef struct _NotifyCallback
{
	void *pCaller;
	PASVidNotify pNotifyFun;
} NotifyCallback, *PNotifyCallback;

/**
 *decoder config information
 */
typedef struct _ASVid_ConfigInfo
{
    int CodecType;     /* codec type, 1-> H264, 2->MPEG2 */
	int nWidth;        /* picture width */
	int nHeight;       /* picture height*/
	int nAvgFrameTime; /* set to modify PTS if there is no PTS. */
	void *pUserData;   /* used for sequence info*/
	int nDataLen;      /* user data len*/
	int nDeviceID;     /* cuda device, -1 : default, let decoder choose. */
	int nOutputMode;   /* must set 0 */
    NotifyCallback fNotifyFun; /* notify function, such as frame out, media information changed */
    void *pReserved[64];
} ASVid_ConfigInfo, *PASVid_ConfigInfo;


/*
 *  config decoder, set call back function, choose decoder core.
 *  pConfig : config param.
 *  pASVid : decoder handle created by decoder core.
 */
int ASVAPI Open(ASVid_ConfigInfo *pConfig, void **pASVid);
typedef int (*pOpen)(ASVid_ConfigInfo *pConfig, void **pASVid);

/*
 *  close decoder after decode finish.
 *  pASVid : decoder handle.
 */
int ASVAPI Close(void *pASVid);
typedef int (*pClose)(void *pASVid);

/*
 *  decoder function.
 *  pASVid : decoder handle.
 *  pData : input bitstream.
 *  nDataLen : input bitstream length.
 *  rtStart : input PTS.
 *  Flags : not used.
 */
int ASVAPI Decode(void *pASVid, unsigned char *pData, long nDataLen, llPTS rtStart, long Flags);
typedef int (*pDecode)(void *pASVid, unsigned char *pData, long nDataLen, llPTS rtStart, long Flags);

/*
 *  Reset Decoder, Decoder will skip all frames until next IDR frame.
 *  pASVid : decoder core.
 */
int ASVAPI ResetDecoder(void *pASVid);
typedef int (*pResetDecoder)(void *pASVid);
/*
 *  Flush Decoder, skip all data.
 *  pASVid : decoder handle.
 *  bBegin : 1->begin flush, 0->end flush.
 */
int ASVAPI Flush(void *pASVid, int bBegin);
typedef int (*pFlush)(void *pASVid, int bBegin);

/*
 *  End Of Bit Stream, output all frames that cached.
 *  pASVid : decoder handle.
 */
int ASVAPI EndOfStream(void *pASVid);
typedef int (*pEndOfStream)(void *pASVid);
/*
 *  Set H264 Decoder Output Mode
 *  pASVid : decoder handle.
 *  mode = 0 : this is default mode, strictly comform with H264 standard 
 *  mode = 1 : normal mode, such as IPBBPBB... ---> IBBPBBP...
 *  mode = 2 : realtime mode, frame decoding order is same with display order, i.e. no B frame
 */
int ASVAPI SetRealtimeOutputMode(void *pASVid, int mode);
typedef int (*pSetRealtimeOutputMode)(void *pASVid, int mode);
/*
 *  Set H264 AVC1.
 *  pASVid : decoder handle.
 *  bAVC1 : 1->AVC1, 0->not AVC1
 *  nFlags : range 1-4
 */
int ASVAPI SetAVC1(void *pASVid, bool bAVC1, int nFlags);
typedef int (*pSetAVC1)(void *pASVid, bool bAVC1, int nFlags);

/*
 *  Used for special function, for further use.
 */
int ASVAPI DecoderSpecialFeature(void *pASVid, int nType, void* pSet, void* pGet);
typedef int (*pDecoderSpecialFeature)(void *pASVid, int nType, void* pSet, void* pGet);

#ifdef __cplusplus
};
#endif
#endif
