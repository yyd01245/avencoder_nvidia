#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<time.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<signal.h>

#include"encodenv.h"
#include<errno.h>

#include<sys/mman.h>




typedef struct _shm_head{

//    int flag;
    int x;
    int y;
    int w;
    int h;

}shm_head;


typedef struct _rgb_mgr{

    char*filename;
    int ifd;
    FILE*ofp;

}rgb_mgr;








int yuv_file_map_read(int fd,unsigned char*oyuv[3],int width,int height)
{
    int ret;

    static size_t file_siz=0;
    static size_t read_offset=0;
    static char*yuvaddr=NULL;


    if(file_siz==0){
        struct stat fs;
        ret=fstat(fd,&fs);
        file_siz=fs.st_size;

        yuvaddr=mmap(NULL,file_siz,PROT_READ,MAP_PRIVATE,fd,0);
        if(yuvaddr==MAP_FAILED){
            fprintf(stderr,"ERROR::Open YUV file: mmap()\n");
            return -1;
        }

    }

    char*luma_addr_start=yuvaddr+read_offset;
    char*chroma_addr_start=luma_addr_start+width*height;
    
    int luma_siz=width*height;
    int chroma_siz=luma_siz>>2;
    
    int i;

    int fsiz=(luma_siz+chroma_siz+chroma_siz);

    if(read_offset+fsiz>file_siz){
#if 1
        fprintf(stderr,"No more data\n");

        munmap(yuvaddr,file_siz);
        file_siz=0;
        read_offset=0;
        yuvaddr=NULL;

        return 0;
#else

        luma_addr_start=yuvaddr;
        chroma_addr_start=luma_addr_start+width*height;
        
        fprintf(stderr,"Reset input data\n");

        read_offset=0;

#endif
    }     


    memcpy(oyuv[0],luma_addr_start,luma_siz);
    memcpy(oyuv[1],chroma_addr_start,chroma_siz);

    chroma_addr_start+=chroma_siz;
    memcpy(oyuv[2],chroma_addr_start,chroma_siz);

    read_offset+=fsiz;

    return fsiz;

}






int rgb_file_map_read(nv_encoder_struct*pencoder,void*data,size_t width,size_t height,size_t filestride,size_t bufferstride)
{
    int nr=0;

    int ret;
    static int file_siz;
    static int rpos=0;
    static unsigned char*rgbs;

    rgb_mgr* mgr=(rgb_mgr*)nv_encoder_get_io_data(pencoder);

    shm_head hd;

    if(mgr->ifd==0){
        mgr->ifd=open(mgr->filename,O_RDONLY);
        if(mgr->ifd<0){
            fprintf(stderr,"ERROR::Open RGB file:open()\n");
            return -1;
        }
        struct stat fs;
        ret=fstat(mgr->ifd,&fs);
        file_siz=fs.st_size;

        fprintf(stderr,"Fsize:%d\n",file_siz);
        rgbs=mmap(NULL,file_siz,PROT_READ,MAP_PRIVATE,mgr->ifd,0);
        close(mgr->ifd);
        if(rgbs==MAP_FAILED){
            fprintf(stderr,"ERROR::Open RGB file: mmap()::{%s}\n",strerror(errno));
//            close(mgr->ifd);
            return -1;
        }
    }

retry:

//read head
//    nr=read(mgr->fd,&hd,sizeof(hd));
    if(sizeof(hd)+rpos>file_siz){
        fprintf(stderr,"rpos:%d\t file_siz:%d\n",rpos,file_siz);
        fprintf(stderr,"===============================iRETRY===============================\n");
#ifdef _TEST_       
        rpos=0;
        goto retry;
#else
        goto unmap;

#endif

    }
    memcpy(&hd,rgbs+rpos,sizeof(hd));
    rpos+=sizeof(hd);

    printf("[x:%d y:%d w:%d h:%d]\n",hd.x,hd.y,hd.w,hd.h);
/*
    enc->st_frame_conf.enc_rect->x=hd.x;
    enc->st_frame_conf.enc_rect->y=hd.y;
    enc->st_frame_conf.enc_rect->w=hd.w;
    enc->st_frame_conf.enc_rect->h=hd.h;
*/
    _frame_conf_set_head(pencoder, hd.x, hd.y, hd.w, hd.h);

//read data
    int real_siz=hd.w*hd.h*4;
    if(real_siz==0){
        fprintf(stderr,"RETRY::Nil REGION\n");
        goto retry;//next head
    }

    if(real_siz+rpos>file_siz){
        fprintf(stderr,"WARN::Read file data less than expect..\n");
#ifdef _TEST_
        rpos=0;
        goto retry;
#else
        goto unmap;
#endif
    }
    memcpy(data,rgbs+rpos,real_siz);

    rpos+=real_siz;
    nr=real_siz;

    return nr;

unmap:

    ret=munmap(rgbs,file_siz);
    if(ret<0){
        fprintf(stderr,"WARN::munmap() Failed..\n");
    }
    rgbs=NULL;
    file_siz=0;
    rpos=0;


    return 0;

}




static int stdio_write_limited(nv_encoder_struct*pencoder,void*data,size_t framesize)
{
    static int nframes=0;
#if 0

    if(nframes>400)
        return 0;
#endif

    int nw;
    rgb_mgr*mgr=nv_encoder_get_io_data(pencoder);

    nw=fwrite(data,1,framesize,mgr->ofp);
    nframes++;

    return nw;
}





int main(argc,argv)
    int argc;
    char**argv;
{

    int idrstart=0;

    if(argc>3)
        idrstart=atoi(argv[3]);

    nv_encoder_struct encoder;
    nv_encoder_config config;

    nv_encoder_get_default_config(&config);

    /* *Change configurations
     *
     * */
    config.avg_bit_rate=2000000;
//    config.field_encoding=1;//=0 frame encodeing

    config.idr_period_startup=idrstart;

    nv_encoder_init(&encoder,&config);


    rgb_mgr mgr;
    bzero(&mgr,sizeof(mgr)); 

    mgr.filename=argv[1];

    FILE* ofp=fopen(argv[2],"wb");
    mgr.ofp=ofp;

    //overwrite <Reader>
    nv_encoder_set_read(&encoder,rgb_file_map_read,&mgr);
    nv_encoder_set_write(&encoder,stdio_write_limited,&mgr);


    int ret;

    while(1){
 
        ret=nv_encoder_encode(&encoder,0);
        if(ret<=0)
            break;
//        usleep(39000);
    }

    fprintf(stdout,"OK..");

    nv_encoder_fini(&encoder);

    fclose(ofp);

    return EXIT_SUCCESS;
}








