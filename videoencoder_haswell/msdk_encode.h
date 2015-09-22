#ifndef MSDK_ENCODE_H
#define MSDK_ENCODE_H
#include "mfxvideo.h"
#include "mfxdefs.h"
#include <va/va.h>
#include "buffdef.h"

typedef struct coded_buf_t{
	unsigned char * buf;
	unsigned int len;
} coded_buf;
typedef struct msdk_encode_task_t{
	mfxBitstream m_bitstream;
	mfxSyncPoint m_syncpoint;
	struct msdk_encode_task_t* next;
} msdk_encode_task;


typedef struct msdk_encode_context_t{
	mfxSession m_session;
	mfxStatus m_status;
	mfxVideoParam m_param;

	mfxExtCodingOption m_coding_option;
	mfxExtPictureTimingSEI m_picture_timing_sei;

	mfxU64 m_timestamp;

	mfxExtBuffer ** m_ext_param;
	mfxFrameAllocRequest m_request;
	VADisplay m_va_dpy;
	int m_fd;
	mfxU16 m_nAsyncDepth;
	mfxFrameSurface1 *m_surface;
	mfxU16 m_surface_num;
	mfxU16 m_last_surface;
	msdk_encode_task * m_task;
} msdk_encode_context;


#define MSDK_ALIGN16(value)                      (((value + 15) >> 4) << 4) // round up to a multiple of 16
#define MSDK_ALIGN32(value)                      (((value + 31) >> 5) << 5) // round up to a multiple of 32

#define CHECK_NO_ERROR(err) assert(err == MFX_ERR_NONE)
#define MSDK_IGNORE_MFX_STS(P, X)	{if ((X) == (P)) {P = MFX_ERR_NONE;}}
#define MSDK_CHECK_POINTER(P, ...)	{if (!(P)) {return __VA_ARGS__;}}
#define MSDK_SLEEP(msec) usleep(1000*msec)

void msdk_encode_init(msdk_encode_context *ctx, int width, int height, int bitrate, int fps, int target);

/**

	please set buf_allocated 0 if out_buf is allocated
	return value: 
	currently return value is always 0, out_buf->len indicates the size 
	*/
int msdk_encode_encode_frame(msdk_encode_context *ctx, unsigned char *yuv_buf, coded_buf *out_buf, int buf_allocated);
void msdk_encode_close(msdk_encode_context *ctx);

void msdk_encode_set_param(msdk_encode_context *ctx, int width, int height, int bitrate, int fps, int target);
mfxStatus ConvertFrameRate(mfxF64 dFrameRate, mfxU32* pnFrameRateExtN, mfxU32* pnFrameRateExtD);

#endif
