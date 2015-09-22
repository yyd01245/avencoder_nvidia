
#include "videosource.h"
#include "videoencoder.h"
#include "audioencoder.h"
#include "audiosource.h"
#include "avmuxer.h"
#include "tssmooth.h"


#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <getopt.h>
#include <errno.h>

#define version_str "1.0.00.15"

const int delaytimeus = 40;
int first_frame = 1;

audio_source_t* p_audio_source = NULL;
video_source_t* p_video_source = NULL;
audio_encoder_t* p_audio_encoder = NULL;
video_encoder_t* p_video_encoder = NULL;
ts_muxer_t* p_avmuxer = NULL;
ts_smooth_t* p_tssmooth = NULL;	

const int n_audio_source_sample_size = 1*1024*1024;
const int n_video_source_sample_size = 5*1024*1024;
char *p_audio_source_sample = NULL;
char *p_video_source_sample = NULL;
int n_audio_source_sample = 0;
int n_video_source_sample = 0;

const int n_audio_encode_sample_size = 1*1024*1024;
const int n_video_encode_sample_size = 5*1024*1024;
char *p_audio_encode_sample = NULL;
char *p_video_encode_sample = NULL;
int n_audio_encode_sample = 0;
int n_video_encode_sample = 0;


int write_video_frames();
int write_audio_frames();
void* statistics_process(void *param);

int m_index = 0;
int width = 1280;
int height = 720;
int encode_bitrate = 4*1024*1024;
int smooth_bitrate = 12*1024*1024;
char destip[32] = "127.0.0.1";
int destport = 14000;
char destip_record[32] = "127.0.0.1";
int destport_record = 14000;
int pmt_pid = 0x1000;
int service_id = 0x0001;
int video_pid = 0x100;
int audio_pid = 0x101;
int gopsize_min = 5;
int gopsize_max = 100;
int fps = 25;
char logfile[256] = "avencoder.log";
	

int main(int argc,char **argv)
{
	
	
	const char *short_options = "i:w:h:b:d:p:s:g:f:l:?";
	struct option long_options[] = {
		{"help",	no_argument,	   NULL, '?'},
		{"index",	required_argument, NULL, 'i'},
		{"width",	required_argument, NULL, 'w'},
		{"height",	required_argument, NULL, 'h'},
		{"bitrate",	required_argument, NULL, 'b'},
		{"sbitrate",required_argument, NULL, 's'},
		{"destaddr",required_argument, NULL, 'd'},
		{"pids",	required_argument, NULL, 'p'},
		{"gopsize",	required_argument, NULL, 'g'},
		{"fps",		required_argument, NULL, 'f'},
		{"logfile",	required_argument, NULL, 'l'},
		{NULL,		0,				   NULL, 0},
	};
	int c = -1;
	bool has_index = false;
	bool has_destaddr = false;
	bool has_ebitrate = false;
	bool has_sbitrate = false;

	while(1)
	{
		c = getopt_long(argc, argv, short_options, long_options, NULL);
		if(c == -1)
			break;
		switch(c)
		{
			case '?':
				printf("Usage:%s [options] index destaddr ....\n",argv[0]);
				printf("avencoder version %s\n",version_str);
				printf("-? --help 		Display this usage information.\n");
				printf("-i --index 		Set the index.\n");
				printf("-w --width 		Set the width.\n");
				printf("-h --height		Set the height.\n");
				printf("-b --bitrate 		Set the encoder bitrate .\n");
				printf("-s --sbitrate 		Set the smooth bitrate.\n");
				printf("-d --destaddr 		Set the dest addr (ip:port).\n");
				printf("-p --pids 		Set the service_id pmt_pid video_pid audio_pid (pmt_pid:service_id:video_pid:audio_pid).\n");
				printf("-g --gopsize 		Set the video encode gopsize (gopsize_min:gopsize_max).\n");
				printf("-l --logfile 		Set the log file path.\n");
				printf("Example: %s --index 0 --width 1280 --height 720 --bitrate 4000000 --pids 1024:1:64:65 --gopsize 5:100 --destaddr 192.168.60.248:14000.\n",argv[0]);
				printf("Example: %s -i 0 -w 1280 -h 720 -b 4000000 -p 1024:1:64:65 -g 5:100 -d 192.168.60.248:14000.\n",argv[0]);
				return -1;
			case 'i':
				m_index = atoi(optarg);
				has_index = true;
				break;
			case 'w':
				width = atoi(optarg);
				break;
			case 'h':
				height = atoi(optarg);
				break;
			case 'b':
				encode_bitrate = atoi(optarg);
				has_ebitrate = true;
				break;
			case 's':
				smooth_bitrate = atoi(optarg);
				has_sbitrate = true;
				break;
			case 'd':
				if(sscanf(optarg,"%[^:]:%d",destip,&destport) != 2)
				{
					fprintf(stderr ,"avencoder: error destip:destport.%s.\n",optarg);        
					return -2; 
				}
				strncpy(destip_record,destip,sizeof(destip_record));
				destport_record = destport;
				has_destaddr = true;
				break;
			case 'p':
				if(sscanf(optarg,"%d:%d:%d:%d",&pmt_pid,&service_id,&video_pid,&audio_pid) != 4)
				{
					fprintf(stderr ,"avencoder: error pmt_pid:service_id:video_pid:audio_pid.%s.\n",optarg);        
					return -3; 
				}
				break;
			case 'g':
				if(sscanf(optarg,"%d:%d",&gopsize_min,&gopsize_max) != 2)
				{
					gopsize_min = gopsize_max = atoi(optarg);
				}
				break;
			case 'f':
				fps = atoi(optarg);
				break;
			case 'l':
				memset(logfile,0,sizeof(logfile));
				strncpy(logfile,optarg,sizeof(logfile));
				break;

			}	
	}

	if(has_index == false || has_destaddr == false)
	{
		fprintf(stderr ,"avencoder: index and destaddr is must,not empty.\n");
		return -3;
	}
	if(has_ebitrate == true && has_sbitrate == false)
		smooth_bitrate = encode_bitrate*3;
	else if(has_ebitrate == false && has_sbitrate == true)
		encode_bitrate = smooth_bitrate/3;

	if(encode_bitrate < 0)
		encode_bitrate = 4*1024*1024;
	
	//TODO 检查各个参数的有效性
	printf("avencoder version %s\n",version_str);
	printf("++++++index=%d|width=%d|height=%d|smooth_bitrate=%d|encode_bitrate=%d|destip=%s|destport=%d++++++\n",m_index,width,height,smooth_bitrate,encode_bitrate,destip,destport);
	printf("++++++pmt_pid=%d|service_id=%d|video_pid=%d|audio_pid=%d|gopsize_min=%d|gopsize_max=%d|logfile=%s++++++\n",pmt_pid,service_id,video_pid,audio_pid,gopsize_min,gopsize_max,logfile);

	InputParams_audiosource audio_source_params;
	InputParams_videosource video_source_params;
	InputParams_videoencoder video_encoder_params;
	InputParams_avmuxer avmuxer_params;
	InputParams_tssmooth tssmooth_params;

	audio_source_params.index = m_index;
	p_audio_source = init_audio_source(&audio_source_params);
	if(p_audio_source == NULL)
	{        
		fprintf(stderr ,"avencoder: init_audio_source fail..\n");        
		return -1;    
	}
	video_source_params.index = m_index;
	video_source_params.width = width;
	video_source_params.height = height;
	
	p_video_source = init_video_source(&video_source_params);
	if(p_video_source == NULL)
	{        
		fprintf(stderr ,"avencoder: init_video_source fail..\n");        
		return -1;    
	}

	p_audio_encoder = init_audio_encoder();
	if(p_audio_encoder == NULL)
	{		 
		fprintf(stderr ,"avencoder: init_audio_encoder fail..\n");		 
		return -1;	  
	}

	
	video_encoder_params.width = width;
	video_encoder_params.height = height;
	video_encoder_params.gopsize_min = gopsize_min;
	video_encoder_params.gopsize_max = gopsize_max;
	video_encoder_params.bitrate = encode_bitrate;
	video_encoder_params.fps = fps;
	p_video_encoder = init_video_encoder(&video_encoder_params);
	if(p_video_encoder == NULL)
	{		 
		fprintf(stderr ,"avencoder: init_video_encoder fail..\n");		 
		return -1;	  
	}
#if 0
	if(smooth_bitrate > 0)
	{
		memset(&tssmooth_params,0,sizeof(tssmooth_params));
		tssmooth_params.index = m_index;
		tssmooth_params.listen_udp_port = 0;
		strcpy(tssmooth_params.dst_udp_ip,destip);
		tssmooth_params.dst_udp_port = destport;
		tssmooth_params.bit_rate = smooth_bitrate;
		tssmooth_params.buffer_max_size = 10*1024*1024;

		p_tssmooth = init_tssmooth(&tssmooth_params);
		if(p_tssmooth == NULL)
		{		 
			fprintf(stderr ,"avencoder: init_smooth fail..\n");		 
			return -1;	  
		}

		strcpy(destip,tssmooth_params.listen_udp_ip);
		destport = tssmooth_params.listen_udp_port;
		
		printf("++++++now dest addr is modify destip=%s|destport=%d++++++\n",destip,destport);
	}
#endif

	avmuxer_params.codecID = KY_CODEC_ID_H264;
	avmuxer_params.nWidth = width;
	avmuxer_params.nHeight = height;
	avmuxer_params.nBitRate = encode_bitrate;
	avmuxer_params.nPeakBitRate = smooth_bitrate;
	avmuxer_params.nSamplerate = 48000;
	avmuxer_params.nFramerate = fps;
	snprintf(avmuxer_params.monitorName,sizeof(avmuxer_params.monitorName),"%d.monitor",m_index);
	snprintf(avmuxer_params.appName,sizeof(avmuxer_params.appName),"%s",argv[0]);

	strcpy(avmuxer_params.destip,destip);
	avmuxer_params.destport = destport;
	avmuxer_params.index = m_index;
	avmuxer_params.pmt_pid = pmt_pid;
	avmuxer_params.service_id = service_id;
	avmuxer_params.video_pid = video_pid;
	avmuxer_params.audio_pid = audio_pid;
	p_avmuxer = init_ts_muxer(&avmuxer_params);
	if(p_avmuxer == NULL)
	{		 
		fprintf(stderr ,"avencoder: init_ts_muxer fail..\n");		 
		return -1;	  
	}



	p_audio_source_sample = (char *)malloc(n_audio_source_sample_size);
	p_video_source_sample = (char *)malloc(n_video_source_sample_size);
	if(p_audio_source_sample == NULL || p_video_source_sample == NULL)
	{		 
		fprintf(stderr ,"avencoder: malloc fail..\n");		 
		return -2;	  
	}

	p_audio_encode_sample = (char *)malloc(n_audio_encode_sample_size);
	p_video_encode_sample = (char *)malloc(n_video_encode_sample_size);
	if(p_audio_encode_sample == NULL || p_video_encode_sample == NULL)
	{		 
		fprintf(stderr ,"avencoder: malloc fail..\n");		 
		return -2;	  
	}

	pthread_t pthread;
	int ret = pthread_create(&pthread, NULL, statistics_process, NULL);
	if(ret != 0)
	{
		printf("avencoder: pthread_create fail!...\n");
		return -3;
	}


	

	double videopts = 0.0;
	double audiopts = 0.0;

	struct timeval tv;
	long start_time_us,end_time_us;
	gettimeofday(&tv,NULL);
	start_time_us = end_time_us = tv.tv_sec*1000 + tv.tv_usec/1000;
	
	while(1)
	{
		gettimeofday(&tv,NULL);
		start_time_us = tv.tv_sec*1000 + tv.tv_usec/1000;

		write_video_frames();

		video_pts(p_avmuxer,&videopts);
		audio_pts(p_avmuxer,&audiopts);

		//printf("++++++++++videopts = [%lf]   audiopts = [%lf]+++++++++\n",videopts,audiopts);

		while(audiopts < videopts)
		{
			write_audio_frames();

			video_pts(p_avmuxer,&videopts);
			audio_pts(p_avmuxer,&audiopts);
			//printf("----------videopts = [%lf]   audiopts = [%lf]---------\n",videopts,audiopts);
		}

		gettimeofday(&tv,NULL);
		end_time_us = tv.tv_sec*1000 + tv.tv_usec/1000;

		//if(end_time_us - start_time_us >= 40)
		//	printf("1++++++++++start_time_us = [%ld]	 end_time_us = [%ld]+++++diff=%ld++++\n",start_time_us,end_time_us,end_time_us - start_time_us);

		if(end_time_us - start_time_us < delaytimeus)
			usleep((delaytimeus-(end_time_us - start_time_us))*1000);
		
	}


	uninit_audio_source(p_audio_source);
	uninit_video_source(p_video_source);
	uninit_audio_encoder(p_audio_encoder);
	uninit_video_encoder(p_video_encoder);
	uninit_ts_muxer(p_avmuxer);
	if(smooth_bitrate > 0 && p_tssmooth)
		uninit_tssmooth(p_tssmooth);


	return 0;
}

int write_video_frames()
{
	int ret = -1;
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
	
	//获取视频源
	n_video_source_sample = n_video_source_sample_size;
	if(first_frame == 0)
	{
		ret = get_video_sample_inc(p_video_source,p_video_source_sample,&n_video_source_sample,&x,&y,&w,&h);
		if(ret < 0)
		{		 
			fprintf(stderr ,"avencoder: get_video_sample_inc fail..ret=%d\n",ret);		 
			return -1;	  
		}
		
	}
	else //first_frame
	{
		ret = get_video_sample_all(p_video_source,p_video_source_sample,&n_video_source_sample,&x,&y,&w,&h);
		if(ret < 0)
		{		 
			fprintf(stderr ,"avencoder: get_video_sample_all fail..ret=%d\n",ret);		 
			return -1;	  
		}
		first_frame = 0;
	}
	
	/*
	struct timeval tv;
	long time_us;
	gettimeofday(&tv,NULL);
	time_us = tv.tv_sec*1000 + tv.tv_usec/1000;
	printf("time_us=%ld x=%d,y=%d,w=%d,h=%d, n_video_source_sample = %d\n",time_us,x,y,w,h,n_video_source_sample);
	*/

	
	//printf("n_video_source_sample = %d\n",n_video_source_sample);

	//编码视频
	n_video_encode_sample = n_video_encode_sample_size;
	ret = encode_video_sample_inc(p_video_encoder,p_video_source_sample,n_video_source_sample,p_video_encode_sample,&n_video_encode_sample,x,y,w,h);
	if(ret < 0)
	{		 
		fprintf(stderr ,"avencoder: encode_video_sample fail..\n");		 
		return -2;	  
	}
	//printf("n_video_encode_sample = %d\n",n_video_encode_sample);
			
	//写视频
	ret = write_video_frame(p_avmuxer,p_video_encode_sample,n_video_encode_sample);
	if(ret < 0)
	{		 
		fprintf(stderr ,"avencoder: write_video_frame fail..\n");		 
		return -3;	  
	}
	return 0;
}

int write_audio_frames()
{
	int ret = 0;
	
	n_audio_source_sample = n_audio_source_sample_size;
	ret = get_audio_sample(p_audio_source,p_audio_source_sample,&n_audio_source_sample);
	if(ret < 0)
	{		 
		fprintf(stderr ,"avencoder: get_audio_sample fail..\n");		 
		return -1;	  
	}
	else if(ret == 0)
	{
		//fprintf(stderr ,"avencoder: get_audio_sample no audio sample..\n");
		n_audio_source_sample = 4608;
		memset(p_audio_source_sample,0,n_audio_source_sample);
	}
	//else if(ret > 0)
	//	fprintf(stderr ,"avencoder: get_audio_sample yes audio sample..\n");

	//printf("n_audio_source_sample = %d\n",n_audio_source_sample);

	n_audio_encode_sample = n_audio_encode_sample_size;
	ret = encode_audio_sample(p_audio_encoder,p_audio_source_sample,n_audio_source_sample,p_audio_encode_sample,&n_audio_encode_sample);
	if(ret < 0)
	{		 
		fprintf(stderr ,"avencoder: encode_audio_sample fail..\n");		 
		return -2;	  
	}

	//printf("n_audio_encode_sample = %d\n",n_audio_encode_sample);


	ret = write_audio_frame(p_avmuxer,p_audio_encode_sample,n_audio_encode_sample);
	if(ret < 0)
	{		 
		fprintf(stderr ,"avencoder: write_audio_frame fail..\n");		 
		return -3;	  
	}
	return 0;
}

void* statistics_process(void *param)
{
	unsigned long read_send_bytes1 = 0;
	unsigned long read_count_fps1 = 0;
	unsigned long read_send_bytes2 = 0;
	unsigned long read_count_fps2 = 0;
	unsigned long read_send_bytes_diff = 0;
	unsigned long read_count_fps_diff = 0;
	unsigned long mask_long = (unsigned long)(-1);
	
	FILE *fp = NULL;
	char filename[256];
	snprintf(filename,sizeof(filename),"/tmp/avencoder.%d",m_index);
	char tmp_buffer[1024];
	
	while(1)
	{
	
	//	if(smooth_bitrate > 0 && p_tssmooth)
	//		read_send_bytes1 = get_real_send_bytes(p_tssmooth);
		if(p_avmuxer)
			read_send_bytes1 = get_bbcvudp_real_send_bytes(p_avmuxer);
		read_count_fps1 = get_real_count_fps(p_video_encoder);
		usleep(1000*1000);
//		if(smooth_bitrate > 0 && p_tssmooth)
//			read_send_bytes2 = get_real_send_bytes(p_tssmooth);
		if(p_avmuxer)
			read_send_bytes2 = get_bbcvudp_real_send_bytes(p_avmuxer);
		read_count_fps2 = get_real_count_fps(p_video_encoder);

		if(read_send_bytes2 < read_send_bytes1)
			read_send_bytes_diff = mask_long - read_send_bytes1 + read_send_bytes2;
		else
			read_send_bytes_diff = read_send_bytes2 - read_send_bytes1;
		
		if(read_count_fps2 < read_count_fps1)
			read_count_fps_diff = mask_long - read_count_fps1 + read_count_fps2;
		else
			read_count_fps_diff = read_count_fps2 - read_count_fps1;

		//printf("read_send_bytes_rate = %lu  read_count_fps = %lu\n",read_send_bytes_diff*8,read_count_fps_diff);

		fp = fopen(filename,"w");
		if(fp == NULL)
		{		 
			fprintf(stderr ,"avencoder: fopen filename %s fail.errno=%d.\n",filename,errno);		 
			continue;	  
		}

		memset(tmp_buffer,0,sizeof(tmp_buffer));
		snprintf(tmp_buffer,sizeof(tmp_buffer),"version=%s\n",version_str);
		fwrite(tmp_buffer,strlen(tmp_buffer),1,fp);
		
		memset(tmp_buffer,0,sizeof(tmp_buffer));
		snprintf(tmp_buffer,sizeof(tmp_buffer),"index=%d\n",m_index);
		fwrite(tmp_buffer,strlen(tmp_buffer),1,fp);

		memset(tmp_buffer,0,sizeof(tmp_buffer));
		snprintf(tmp_buffer,sizeof(tmp_buffer),"destaddr=%s:%d\n",destip_record,destport_record);
		fwrite(tmp_buffer,strlen(tmp_buffer),1,fp);

		memset(tmp_buffer,0,sizeof(tmp_buffer));
		snprintf(tmp_buffer,sizeof(tmp_buffer),"destaddr_local=%s:%d\n",destip,destport);
		fwrite(tmp_buffer,strlen(tmp_buffer),1,fp);

		memset(tmp_buffer,0,sizeof(tmp_buffer));
		snprintf(tmp_buffer,sizeof(tmp_buffer),"realbitrate=%lu\n",read_send_bytes_diff*8);
		fwrite(tmp_buffer,strlen(tmp_buffer),1,fp);

		memset(tmp_buffer,0,sizeof(tmp_buffer));
		snprintf(tmp_buffer,sizeof(tmp_buffer),"realfps=%lu\n",read_count_fps_diff);
		fwrite(tmp_buffer,strlen(tmp_buffer),1,fp);

		memset(tmp_buffer,0,sizeof(tmp_buffer));
		snprintf(tmp_buffer,sizeof(tmp_buffer),"encode_bitrate=%d\n",encode_bitrate);
		fwrite(tmp_buffer,strlen(tmp_buffer),1,fp);

		memset(tmp_buffer,0,sizeof(tmp_buffer));
		snprintf(tmp_buffer,sizeof(tmp_buffer),"smooth_bitrate=%d\n",smooth_bitrate);
		fwrite(tmp_buffer,strlen(tmp_buffer),1,fp);

		memset(tmp_buffer,0,sizeof(tmp_buffer));
		snprintf(tmp_buffer,sizeof(tmp_buffer),"fps=%d\n",fps);
		fwrite(tmp_buffer,strlen(tmp_buffer),1,fp);
		
		memset(tmp_buffer,0,sizeof(tmp_buffer));
		snprintf(tmp_buffer,sizeof(tmp_buffer),"pmt_pid=%d\n",pmt_pid);
		fwrite(tmp_buffer,strlen(tmp_buffer),1,fp);

		memset(tmp_buffer,0,sizeof(tmp_buffer));
		snprintf(tmp_buffer,sizeof(tmp_buffer),"service_id=%d\n",service_id);
		fwrite(tmp_buffer,strlen(tmp_buffer),1,fp);

		memset(tmp_buffer,0,sizeof(tmp_buffer));
		snprintf(tmp_buffer,sizeof(tmp_buffer),"video_pid=%d\n",video_pid);
		fwrite(tmp_buffer,strlen(tmp_buffer),1,fp);

		memset(tmp_buffer,0,sizeof(tmp_buffer));
		snprintf(tmp_buffer,sizeof(tmp_buffer),"audio_pid=%d\n",audio_pid);
		fwrite(tmp_buffer,strlen(tmp_buffer),1,fp);

		memset(tmp_buffer,0,sizeof(tmp_buffer));
		snprintf(tmp_buffer,sizeof(tmp_buffer),"gopsize=%d:%d\n",gopsize_min,gopsize_max);
		fwrite(tmp_buffer,strlen(tmp_buffer),1,fp);

		memset(tmp_buffer,0,sizeof(tmp_buffer));
		snprintf(tmp_buffer,sizeof(tmp_buffer),"width=%d\n",width);
		fwrite(tmp_buffer,strlen(tmp_buffer),1,fp);

		memset(tmp_buffer,0,sizeof(tmp_buffer));
		snprintf(tmp_buffer,sizeof(tmp_buffer),"height=%d\n",height);
		fwrite(tmp_buffer,strlen(tmp_buffer),1,fp);

		fflush(fp);
		fclose(fp);
		
	}
}


