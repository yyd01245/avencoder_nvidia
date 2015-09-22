#include<stdio.h>
#include<stdlib.h>


void checkCUDAError(const char *msg)                                                                                                                                   
{
    cudaError_t err = cudaGetLastError();
    if( cudaSuccess != err ){
        fprintf(stderr, "ERROR[CUDA]:%s{%s}.\n", msg, cudaGetErrorString( err ) );
        exit(EXIT_FAILURE);                                
    }
}



//Copy RGB data from shared memory region..
void copy_shmrgb_to_device(unsigned char*rgbs,
        unsigned char*devmem,//already allocated throuth cuMemAlloc()
        int rgbleft,int rgbtop,
        int rgbwidth,int rgbheight,
        int width,int height)
{

    int offset=(rgbtop*width)<<2;
    int offset_left=rgbleft<<2;
    
    int line_siz=width<<2;

    int h=0;

    for(h=rgbtop;h<rgbheight+rgbtop;h++){
        cudaMemcpy(devmem+offset+offset_left,rgbs+offset+offset_left,rgbwidth<<2,cudaMemcpyHostToDevice);
        offset+=line_siz;
    }

}




//for TEST ONLY,
void copy_caprgb_to_device(unsigned char*rgbs,
        unsigned char*devmem,//already allocated throuth cuMemAlloc()
        int patch_left,int patch_top,
        int patch_width,int patch_height,
        int width,int height)
{

    int rgb_offset=0;

    int offset=(patch_top*width)<<2;
    int offset_left=patch_left<<2;
    
    int line_siz=width<<2;

    int h;

    for(h=0;h<patch_height;h++){
        cudaMemcpy(devmem+offset+offset_left,rgbs+rgb_offset,patch_width<<2,cudaMemcpyHostToDevice);
        offset+=line_siz;
        rgb_offset+=(patch_width<<2);
    }

}







__global__ void 
convert_line_rgb_to_nv12(unsigned char*devrgb,int rgbstride,/*device mem*/
        unsigned char*oyuv,int ostride,int ovstride,/*device mem*/
        int width,int left,int top)
{
    int curline=threadIdx.x;

    unsigned char*rgb_p=devrgb+(curline+top)*rgbstride*4;
    unsigned char*luma_p=oyuv+(curline+top)*ostride;
    unsigned char*chroma_p=oyuv+(ovstride*ostride)+((curline+top)>>1)*ostride;

    int r,g,b;
    int y,u,v;

    int j;
    if(curline%2==0){
    //even line
        for(j=left;j<width+left;j++){

            b=*(rgb_p+j*4);
            g=*(rgb_p+j*4+1);
            r=*(rgb_p+j*4+2);

            y= 0.299*r + 0.587*g + 0.114*b;
            *(luma_p+j)=(char)y&0xff;

            if(j%2==0){
                u= -0.169*r - 0.331*g + 0.5*b+128;
                *(chroma_p+j)=(char)u&0xff;
            }
            
        }

    }else{
    //odd line

         for(j=left;j<width+left;j++){

            b=*(rgb_p+j*4);
            g=*(rgb_p+j*4+1);
            r=*(rgb_p+j*4+2);

            y= 0.299*r + 0.587*g + 0.114*b;
            *(luma_p+j)=(char)y&0xff;
          
            if(j%2==0){
                v= 0.5*r - 0.419*g - 0.081*b+128;
                *(chroma_p+j+1)=(char)v&0xff;
            } 
        }

    }

}





//FIXME

__global__ void 
convert_line_yv12_to_nv12(unsigned char*pdev,int istride,
        unsigned char*oyuv,int ostride,
        int width,int height)
{
    int curline=threadIdx.x;

    int yv12_luma_siz = istride*height;
    int yv12_chrome_siz = yv12_luma_siz>>2;

    int curpos=curline*istride;


    unsigned char*yv12_luma_p=pdev+curpos;
    unsigned char*yv12_v_p=pdev+yv12_luma_siz+(curpos>>1);
    unsigned char*yv12_u_p=pdev+yv12_luma_siz+yv12_chrome_siz+(curpos>>1);

    curpos=curline*ostride;

    unsigned char*nv12_luma_p=oyuv+curpos;
    unsigned char*nv12_chrome_p=oyuv+(height*ostride)+(curpos>>1);


    char val;

    int j;
    for(j=0;j<width;j++){

        val=*(yv12_luma_p+j);
        *(nv12_luma_p+j)=val;

        val=*(yv12_u_p+j);
        *(nv12_chrome_p)=val;

        val=*(yv12_v_p+j);
        *(nv12_chrome_p+1)=val;
    }

}












extern "C" void load_rgb_bgrx_cuda(
        unsigned char* oyuv,/*device*/
        unsigned char* devrgb,/*device */
        unsigned char*rgb, /*input data host*/
        int left,int top,int width,int height,//rgb patch rect
        int rgbwidth,int rgbheight,//rgb data size
        int ostride //yuv data height<pixel>
        )
{

    //Copy date from shared Memory to Device;
#if 0
    // Read rects from shm region.
    copy_shmrgb_to_device((unsigned char*)rgb,
        (unsigned char*)devrgb,//already allocated throuth cuMemAlloc()
        left,top,
        width,height,
        rgbwidth,rgbheight);

#else
    //for TEST :read rects from capture file.
    copy_caprgb_to_device((unsigned char*)rgb,
        (unsigned char*)devrgb,//already allocated throuth cuMemAlloc()
        left,top,
        width,height,
        rgbwidth,rgbheight);

#endif


    int ovstride=rgbheight;

//    fprintf(stderr,"rgbwidth:%d ostride:%d ovstride:%d, width:%d, left:%d, top:%d\n",rgbwidth,ostride,ovstride,width,left,top);
    convert_line_rgb_to_nv12<<<1,height>>>(devrgb,rgbwidth,
                                                oyuv,ostride,ovstride,
                                                width,left,top);

    cudaThreadSynchronize();

    checkCUDAError("Convert BGRA to NV12\n");

}






extern "C" void load_yuv_yv12_cuda(
        unsigned char* oyuv,/*device*/
        unsigned char* devyv12,/*device */
        unsigned char*iyuv, /*input data host*/
        int width,int height,/*real size*/
        int istride,int ostride
        )
{



//  Load yv12 to device buffer
//TODO

    int in_luma_siz=istride*height;
    int out_luma_siz=ostride*height;
    int in_chroma_siz=in_luma_siz>>2;
    int out_chroma_siz=out_luma_siz>>2;

    unsigned char*in_luma_p=iyuv;
    unsigned char*out_luma_p=devyv12;


    unsigned char*in_v_p=iyuv+in_luma_siz;
    unsigned char*out_v_p=devyv12+out_luma_siz;

    unsigned char*in_u_p=iyuv+in_luma_siz+in_chroma_siz;
    unsigned char*out_u_p=devyv12+out_luma_siz+out_chroma_siz;


    int j;

    for(j=0;j<height;j++){
        //y
        memcpy(out_luma_p+j*ostride,in_luma_p+j*istride,width);
    }

    for(j=0;j<(height>>1);j++){
        //v
        memcpy(out_v_p+((j*ostride)>>1),in_v_p+((j*istride)>>1),width>>1);
        //u
        memcpy(out_u_p+((j*ostride)>>1),in_u_p+((j*istride)>>1),width>>1);
    }




//    fprintf(stderr,"rgbwidth:%d ostride:%d ovstride:%d, width:%d, left:%d, top:%d\n",rgbwidth,ostride,ovstride,width,left,top);
    convert_line_yv12_to_nv12<<<1,height>>>(devyv12,istride,
                                                oyuv,ostride,
                                                width,height);

    cudaThreadSynchronize();

    checkCUDAError("Convert YV12 to NV12\n");

}










