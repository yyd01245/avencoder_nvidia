#include "audioencoder.h"

#ifndef INT64_C
#define INT64_C(c) c##LL
#endif
#ifndef UINT64_C
#define UINT64_C(c) c##LL
#endif

extern "C" {
	#include "libavcodec/avcodec.h"
	#include "libavformat/avformat.h"
	#include "libavformat/avio.h"
	#include "libswscale/swscale.h"
	#include "libavutil/common.h"
}

#include <string.h>
#include <stdlib.h>

typedef struct
{
	AVOutputFormat *m_pOutputFmt;    
	AVFormatContext *m_pFormatCtx;
	AVCodecContext *m_pCodecCtx;
	AVStream *m_pAudioStream;
	uint8_t *tmp_buffer;
	int tmp_buffer_size;
}audioencoder_instanse;

audio_encoder_t* init_audio_encoder()
{
	av_register_all();

	AVOutputFormat *m_pOutputFmt = NULL;    
	AVFormatContext *m_pFormatCtx = NULL;
	AVCodecContext *c = NULL;
	AVStream *m_pAudioStream = NULL;

	m_pOutputFmt = av_guess_format("mpegts", NULL, NULL);
	m_pFormatCtx = avformat_alloc_context();
	if (!m_pFormatCtx)
	{
		fprintf(stderr ,"libaudioencoder: avformat_alloc_context error\n");
		return NULL;
	}
	m_pFormatCtx->oformat = m_pOutputFmt;

	m_pAudioStream = avformat_new_stream(m_pFormatCtx, NULL);    
	if (!m_pAudioStream) 
	{
		fprintf(stderr ,"libaudioencoder: avformat_new_stream error\n");
		return NULL;
	}

	c = m_pAudioStream->codec;
    c->codec_id = m_pOutputFmt->audio_codec;
    c->codec_type	= AVMEDIA_TYPE_AUDIO;
    c->sample_rate = 48000;
    c->bit_rate    = 64000;
    c->channels    = 2;
	c->channel_layout = AV_CH_LAYOUT_STEREO;
    if(c->codec_id == CODEC_ID_VORBIS) {
        c->sample_fmt = AV_SAMPLE_FMT_FLT; // OGG assumes samples are of type float..... input raw PCM data should be 32 bit float
    } else {
        c->sample_fmt = AV_SAMPLE_FMT_S16; // This assumes the input raw PCM data samples are of type 16 bit signed integer
    }
    // some formats want stream headers to be separate
    if (m_pFormatCtx->oformat->flags & AVFMT_GLOBALHEADER) {
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }

    // find the audio encoder
    AVCodec *codec = avcodec_find_encoder(c->codec_id);
    if (!codec) 
	{
        fprintf(stderr ,"libaudioencoder: audio codec not found\n");
        return NULL;
    }
    if (avcodec_open2(c, codec, NULL) < 0) 
	{
        fprintf(stderr ,"libaudioencoder: could not open the audio codec\n");
        return NULL;
    }


	av_dump_format(m_pFormatCtx, 0, " ", 1);

	audioencoder_instanse *p_instanse = (audioencoder_instanse *)malloc(sizeof(audioencoder_instanse));
	if(p_instanse == NULL)
		return NULL;
	p_instanse->m_pAudioStream = m_pAudioStream;
	p_instanse->m_pCodecCtx = c;
	p_instanse->m_pFormatCtx = m_pFormatCtx;
	p_instanse->m_pOutputFmt = m_pOutputFmt;

	p_instanse->tmp_buffer_size = 10*1024;
	p_instanse->tmp_buffer = (uint8_t *)malloc(p_instanse->tmp_buffer_size);
	if(p_instanse->tmp_buffer == NULL)
	{
        fprintf(stderr ,"libaudioencoder: malloc fail\n");
        return NULL;
    }
	
	return p_instanse;
}

int encode_audio_sample(audio_encoder_t *audioencoder,const char *input_audio_sample,int input_audio_sample_size,
	char *output_audio_sample,int *output_audio_sample_size)
{
	if(audioencoder == NULL || input_audio_sample == NULL || output_audio_sample == NULL || output_audio_sample_size == NULL)
	{        
		fprintf(stderr ,"libaudioencoder: Error paraments..\n");        
		return -1;    
	}

	audioencoder_instanse *p_instanse = (audioencoder_instanse *)audioencoder;
	AVCodecContext *c = p_instanse->m_pAudioStream->codec;
	
	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.size = p_instanse->tmp_buffer_size;
	pkt.data = p_instanse->tmp_buffer;
	
	AVFrame aFrame;    
	aFrame.data[0] = (uint8_t *)input_audio_sample;
	aFrame.extended_data = aFrame.data;
	aFrame.pts = AV_NOPTS_VALUE;    
	aFrame.nb_samples	= c->frame_size;    
	int got_packet;    
	int res = avcodec_encode_audio2(c, &pkt, &aFrame, &got_packet);    
	if (res != 0) 
	{        
		fprintf(stderr ,"libaudioencoder: Error encoding audio\n");        
		return -3;    
	}    
	if (!got_packet) 
	{		
		fprintf(stderr ,"libaudioencoder: Error encoding audio while getting packet\n");        
		return -3;    
	}

	if(*output_audio_sample_size < pkt.size)
	{        
		fprintf(stderr ,"libaudioencoder: Error output_audio_sample_size is too small..\n");        
		return -4;    
	} 
	
	memcpy(output_audio_sample,pkt.data,pkt.size);
	*output_audio_sample_size = pkt.size;
	
	return 0;
}


int uninit_audio_encoder(audio_encoder_t *audioencoder)
{
	if(audioencoder == NULL)
	{        
		fprintf(stderr ,"libaudioencoder: Error paraments..\n");        
		return -1;    
	}
	audioencoder_instanse *p_instanse = (audioencoder_instanse *)audioencoder;

	if (p_instanse->m_pAudioStream) 
	{
        avcodec_close(p_instanse->m_pAudioStream->codec);
    }
	
	/* Free the streams. */
    for (int i = 0; i < p_instanse->m_pFormatCtx->nb_streams; i++) {
        av_freep(&p_instanse->m_pFormatCtx->streams[i]->codec);
        av_freep(&p_instanse->m_pFormatCtx->streams[i]);
    }

    if (!(p_instanse->m_pOutputFmt->flags & AVFMT_NOFILE))
        /* Close the output file. */
        avio_close(p_instanse->m_pFormatCtx->pb);

    /* free the stream */
    av_free(p_instanse->m_pFormatCtx);

	if(p_instanse->tmp_buffer != NULL)
		free(p_instanse->tmp_buffer);
	free(p_instanse);
	
	return 0;
}




