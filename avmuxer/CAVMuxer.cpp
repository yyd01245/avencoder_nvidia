#include "CAVMuxer.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>


#define STREAM_FRAME_RATE 25 /* 25 f/s */
static int sws_flags = SWS_BICUBIC;


CAVMuxer::CAVMuxer(void)
{
    m_pOutputFmt = NULL;
    m_pFormatCtx = NULL;
    m_pVideoStream = NULL;
    m_pAudioStream = NULL;
    m_pParams = NULL;

	m_nVideoFrameCount = 0;
	
}

CAVMuxer::~CAVMuxer(void)
{
    Close();
}

int CAVMuxer::Init(InputParams* pInputParam)
{
	m_pParams = (InputParams*)malloc(sizeof(InputParams));

	memcpy(m_pParams,pInputParam,sizeof(InputParams));
    //m_pParams = pInputParam;

	char ouputName[256];
	memset(ouputName,0,sizeof(ouputName));
//	snprintf(ouputName,sizeof(ouputName),"udp://%s:%d?pkt_size=188",m_pParams->destip,m_pParams->destport);
//	snprintf(ouputName,sizeof(ouputName),"udp://%s:%d?pkt_size=1316?buffer_size=0",m_pParams->destip,m_pParams->destport);
	snprintf(ouputName,sizeof(ouputName),"bbcvudp://%s:%d?bit_rate=%d&pkt_size=1316&buffer_size=0",m_pParams->destip,
		m_pParams->destport,m_pParams->nPeakBitRate);

    av_register_all();
    avformat_network_init();
    printf("m_pParams monitor is %s \n",m_pParams->monitorName);
    /* allocate the output media context */
    /*avformat_alloc_output_context2(&m_pFormatCtx, NULL, "mpegts", filename);
    if (!m_pFormatCtx) {
    fprintf(stderr, "Could not deduce output format '%s'\n", filename);
    return 0;
    }
    m_pOutputFmt = m_pFormatCtx->oformat;
    */
    m_pOutputFmt = av_guess_format("mpegts", NULL, NULL);
    CodecID curCodecID;
    if (m_pParams->codecID == KY_CODEC_ID_MPEG2VIDEO) {
        curCodecID = CODEC_ID_MPEG2VIDEO;
    } else if (m_pParams->codecID == KY_CODEC_ID_H264) {
        curCodecID = CODEC_ID_H264;
    } else {  //default
        curCodecID = CODEC_ID_MPEG2VIDEO;
    }
    m_pOutputFmt->video_codec = curCodecID;
	m_pOutputFmt->audio_codec = CODEC_ID_MP2;
    m_pFormatCtx = avformat_alloc_context();
    if (!m_pFormatCtx) {
        fprintf(stderr, "FFMPEG: avformat_alloc_context error\n");
        return -1;
    }
    m_pFormatCtx->oformat = m_pOutputFmt;
    strcpy(m_pFormatCtx->filename, ouputName);

    if (m_pOutputFmt->video_codec != CODEC_ID_NONE) {
        AddVideoStream();
    }
    if (m_pOutputFmt->audio_codec != CODEC_ID_NONE) {
        AddAudioStream();
    }

    av_dump_format(m_pFormatCtx, 0, ouputName, 1);
    /* open the output file, if needed */
    if (!(m_pOutputFmt->flags & AVFMT_NOFILE)) {
        if (avio_open(&m_pFormatCtx->pb, ouputName, AVIO_FLAG_WRITE) < 0) {
            fprintf(stderr, "Could not open '%s'\n", ouputName);
            return 1;
        }
    }
	fprintf(stderr, "open sucess '%s'\n", ouputName);


    AVDictionary *options = NULL;
	/*
	av_dict_set(&options, "mpegts_transport_stream_id", "1", 0);
	av_dict_set(&options, "mpegts_original_network_id", "1", 0);
	av_dict_set(&options, "mpegts_service_id", "1", 0);
	av_dict_set(&options, "mpegts_pmt_start_pid", "4096", 0);
	av_dict_set(&options, "mpegts_start_pid", "256", 0);
	av_dict_set(&options, "muxrate", "1", 0);
	*/

	char pmt_pid_str[64];
	char service_id_str[64];
	char video_pid_str[64];
	char audio_pid_str[64];
	//char muxrate_str[64];

	snprintf(pmt_pid_str,sizeof(pmt_pid_str),"%d",pInputParam->pmt_pid);
	snprintf(service_id_str,sizeof(service_id_str),"%d",pInputParam->service_id);
	snprintf(video_pid_str,sizeof(video_pid_str),"%d",pInputParam->video_pid);
	snprintf(audio_pid_str,sizeof(audio_pid_str),"%d",pInputParam->audio_pid);
	//snprintf(muxrate_str,sizeof(muxrate_str),"%d",pInputParam->nBitRate);

	av_dict_set(&options, "mpegts_use_this_pid","1", 0);
	av_dict_set(&options, "mpegts_pmt_pid"  ,pmt_pid_str  , 0);
	av_dict_set(&options, "mpegts_service_id"  ,service_id_str  , 0);
	av_dict_set(&options, "mpegts_video_pid",video_pid_str, 0);
	av_dict_set(&options, "mpegts_audio_pid",audio_pid_str, 0);
	//av_dict_set(&options, "muxrate",muxrate_str, 0);

    /* Write the stream header, if any. */
    if (avformat_write_header(m_pFormatCtx, &options)) {
         fprintf(stderr ,"FFMPEG: avformat_write_header error!\n");
		 av_dict_free(&options);
         return -1;
    }
	av_dict_free(&options);
	return 0;
}

bool CAVMuxer::AddVideoStream()
{
	m_pVideoStream = av_new_stream(m_pFormatCtx, 0);
	if (!m_pVideoStream) {
	    fprintf(stderr ,"FFMPEG: Could not alloc video stream\n");
	    return false;
	}
	AVCodecContext *c = m_pVideoStream->codec;
	c->codec_id = m_pOutputFmt->video_codec;
	c->codec_type = AVMEDIA_TYPE_VIDEO;
	c->bit_rate = m_pParams->nBitRate;
	c->rc_min_rate = c->bit_rate;
	c->rc_max_rate = c->bit_rate;
	c->bit_rate_tolerance = c->bit_rate;
	c->rc_buffer_size = c->bit_rate;
	c->rc_initial_buffer_occupancy = c->rc_buffer_size*3/4;
	//c->rc_buffer_aggressivity= (float)1.0;
	//c->rc_initial_cplx= 0.5;
	c->width = m_pParams->nWidth;
	c->height = m_pParams->nHeight;
	c->time_base.den = STREAM_FRAME_RATE;
	c->time_base.num = 1;
	c->gop_size      = 12; /* emit one intra frame every twelve frames at most */
	// Some formats want stream headers to be separate
	if(m_pFormatCtx->oformat->flags & AVFMT_GLOBALHEADER) {
	    c->flags |= CODEC_FLAG_GLOBAL_HEADER;
	}
	
	fprintf(stderr,"AddVideoStream: add video stream success:codec_id=%d,width=%d, height=%d,bitrate=%d\n", c->codec_id, c->width,c->height,c->bit_rate);
    return true;
}

bool CAVMuxer::AddAudioStream()
{
    m_pAudioStream = av_new_stream(m_pFormatCtx, 1);
    if (!m_pAudioStream) {
        fprintf(stderr, "Could not alloc stream\n");
        return false;
    }

    AVCodecContext *c = m_pAudioStream->codec;
    c->codec_id = m_pOutputFmt->audio_codec;
    c->codec_type	= AVMEDIA_TYPE_AUDIO;
    c->sample_rate = m_pParams->nSamplerate;
    c->bit_rate    = 64000;
    c->channels    = 2;
    if(c->codec_id == CODEC_ID_VORBIS) {
        c->sample_fmt = AV_SAMPLE_FMT_FLT; // OGG assumes samples are of type float..... input raw PCM data should be 32 bit float
    } else {
        c->sample_fmt = AV_SAMPLE_FMT_S16; // This assumes the input raw PCM data samples are of type 16 bit signed integer
    }
    // some formats want stream headers to be separate
    if (m_pFormatCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }
	
	fprintf(stderr,"AddAudioStream: add audio stream success:codec_id=%d\n",c->codec_id);
    return 0;
}

int CAVMuxer::WriteVideoFrame(char* buff, int size)
{
	int ret;
	AVCodecContext *vc = m_pVideoStream->codec;
	
	AVPacket pkt;
	av_init_packet(&pkt);
	
	m_pVideoStream->pts.val = m_nVideoFrameCount;
	pkt.pts = av_rescale_q(m_pVideoStream->pts.val, vc->time_base, m_pVideoStream->time_base);
	//pkt.dts	 = AV_NOPTS_VALUE;
	pkt.stream_index = m_pVideoStream->index;
	pkt.data = (unsigned char*)buff;
	pkt.size = size;
	/*if(frameType == (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR))
	pkt.flags |= AV_PKT_FLAG_KEY;*/
    if ((ret = av_interleaved_write_frame(m_pFormatCtx, &pkt)) != 0) {
        fprintf(stderr ,"FFMPEG: Error while writing video frame\n");
        return -1;
    }
	m_nVideoFrameCount++;
	
	return 0;
}

int CAVMuxer::WriteAudioFrame(char* buff, int size)
{
    AVCodecContext *c = m_pAudioStream->codec;
	
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = (unsigned char*)buff;
    pkt.size = size;
    pkt.flags |= AV_PKT_FLAG_KEY;
    pkt.stream_index = m_pAudioStream->index;
    //pkt.pts = av_rescale_q(m_curAudioPts, c->time_base, m_pAudioStream->time_base);


    /* Write the compressed frame to the media file. */
    if (av_interleaved_write_frame(m_pFormatCtx, &pkt) != 0) {
        fprintf(stderr, "Error while writing audio frame\n");
        return -1;
    }
	return 0;
}

double CAVMuxer::GetAudioPts()
{
	double realAudioPts = (double)m_pAudioStream->pts.val * m_pAudioStream->time_base.num / m_pAudioStream->time_base.den;
	return realAudioPts;
}

double CAVMuxer::GetVideoPts()
{
	double realVideoPts = (double)m_pVideoStream->pts.val * m_pVideoStream->time_base.num / m_pVideoStream->time_base.den;
	return realVideoPts;
}

unsigned long CAVMuxer::GetSendBytes()
{
	unsigned long realSendbytes = 0;
	avio_getbbcv_udp_realbytes(m_pFormatCtx->pb,&realSendbytes);
//	printf("--send data %ld \n",realSendbytes);
	return realSendbytes;
}


void CAVMuxer::Close()
{
    //flush_audio(m_pFormatCtx);
    av_write_trailer(m_pFormatCtx);

    /* Close each codec. */
    if (m_pVideoStream) {
        avcodec_close(m_pVideoStream->codec);
    }
    if (m_pAudioStream) {
        avcodec_close(m_pAudioStream->codec);
    }

    /* Free the streams. */
    for (int i = 0; i < m_pFormatCtx->nb_streams; i++) {
        av_freep(&m_pFormatCtx->streams[i]->codec);
        av_freep(&m_pFormatCtx->streams[i]);
    }

    if (!(m_pOutputFmt->flags & AVFMT_NOFILE))
        /* Close the output file. */
        avio_close(m_pFormatCtx->pb);

    /* free the stream */
    av_free(m_pFormatCtx);

	free(m_pParams);

}


