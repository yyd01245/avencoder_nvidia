
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

//#include"formats.h"


void checkCUDAError(const char *msg)                                                                                                                                   
{
    cudaError_t err = cudaGetLastError();
    if( cudaSuccess != err ){
        fprintf(stderr, "ERROR[CUDA]:%s{%s}.\n", msg, cudaGetErrorString( err ) );
        exit(EXIT_FAILURE);                                
    }
}



//Copy RGB data from shared memory region..
inline void copy_shmrgb_to_device(unsigned char*rgbs,
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
inline void copy_caprgb_to_device(unsigned char*rgbs,
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
    int yv12_chroma_siz = yv12_luma_siz>>2;

    int curpos=curline*istride;


    unsigned char*yv12_luma_p=pdev+curpos;
    unsigned char*yv12_v_p=pdev+yv12_luma_siz+(curpos>>1);
    unsigned char*yv12_u_p=pdev+yv12_luma_siz+yv12_chroma_siz+(curpos>>1);

    curpos=curline*ostride;

    unsigned char*nv12_luma_p=oyuv+curpos;
    unsigned char*nv12_chroma_p=oyuv+(height*ostride)+(curpos>>1);


    char val;

    int j;
    for(j=0;j<width;j++){

        val=*(yv12_luma_p+j);
        *(nv12_luma_p+j)=val;

        val=*(yv12_u_p+j);
        *(nv12_chroma_p)=val;

        val=*(yv12_v_p+j);
        *(nv12_chroma_p+1)=val;
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
#if 1
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


/***************************************************/
/***************************************************/
/***************************************************/
/***************************************************/



extern"C"{


inline void rgb2yuv_pixel(
        unsigned char r,
        unsigned char g,
        unsigned char b,
        unsigned char*y,
        unsigned char*u,
        unsigned char*v
        ){
#if 0
        //YCbCr
            *y=(0.257*r)+(0.504*g)+(0.098*b)+16;
            *u=-(0.148 * r) - (0.291 * g) + (0.439 * b) + 128;
            *v=(0.439*r)-(0.368*g)+(0.071*b)+128;
#else
        //YUV Intel IPP的BT.709
            *y= 0.299*r + 0.587*g + 0.114*b;
            *u= -0.169*r - 0.331*g + 0.5*b+128;
            *v= 0.5*r - 0.419*g - 0.081*b+128;
#endif

}




/*For Test*/
void load_rgb_bgrx_(unsigned char*yuv,unsigned char*rgb,
        int left,int top,int width,int height,//patch rectangle
        int rgbheight,
        int ostride)

{
    //assert left top width height are even;
    //
    int luma_off=ostride*rgbheight;

    unsigned char*luma_p;
    unsigned char*chroma_p;
    unsigned char*rgb_p;
    

    int r,g,b;
    int y,u,v;
//    fprintf(stderr,"LOAD {x:%d, y:%d, w:%d, h:%d, ww:%d, hh:%d }\n",left,top,width,height,stride,vstride);

    int i,j;
    for(i=top;i<height+top;i++){
        //rows
        rgb_p=rgb+width*(i-top)*4;
        luma_p=yuv+ostride*i;
        chroma_p=yuv+luma_off+ostride*(i/2);

        for(j=left;j<width+left;j++){
#if 1
            b=*(rgb_p+(j-left)*4);
            g=*(rgb_p+(j-left)*4+1);
            r=*(rgb_p+(j-left)*4+2);
#else
#endif

            y= 0.299*r + 0.587*g + 0.114*b;
            *(luma_p+j)=(char)y&0xff;

            if(i%2==0 && j%2==0){
                u= -0.169*r - 0.331*g + 0.5*b+128;
                *(chroma_p+j)=(char)u&0xff;
            }
            
            if(i%2==1 && j%2==0){
                v= 0.5*r - 0.419*g - 0.081*b+128;
                *(chroma_p+j+1)=(char)v&0xff;
            } 
        }
    }
}





void load_rgb_bgrx_2(unsigned char*yuv,unsigned char*rgb,
        int left,int top,int width,int height,//patch rectangle
        int rgbheight,
        int ostride)

{
    //assert left top width height are even;
    //
    int luma_off=ostride*rgbheight;

    unsigned char*luma_p0,*luma_p1;
    unsigned char*chroma_p;
    unsigned char*rgb_p0,*rgb_p1;
   
    int au;//(u1+u2+u3+u4)/4
    int av;//

    unsigned char r,g,b;
    unsigned char y,u,v;
//    fprintf(stderr,"LOAD {x:%d, y:%d, w:%d, h:%d, ww:%d, hh:%d }\n",left,top,width,height,stride,vstride);

    int i,j;
    for(i=top;i<height+top;i+=2){
        //rows
        rgb_p0=rgb+width*(i-top)*4;
        rgb_p1=rgb+width*(i-top+1)*4;
        luma_p0=yuv+ostride*i;
        luma_p1=yuv+ostride*(i+1);
        chroma_p=yuv+luma_off+ostride*(i/2);

        for(j=left;j<width+left;j++){

            b=*(rgb_p0+(j-left)*4);
            g=*(rgb_p0+(j-left)*4+1);
            r=*(rgb_p0+(j-left)*4+2);

            rgb2yuv_pixel(r,g,b,&y,&u,&v);
            *(luma_p0+j)=(char)y&0xff;
            au+=u;
            av+=v;
///////////
            b=*(rgb_p1+(j-left)*4);
            g=*(rgb_p1+(j-left)*4+1);
            r=*(rgb_p1+(j-left)*4+2);

            rgb2yuv_pixel(r,g,b,&y,&u,&v);
            *(luma_p1+j)=(char)y&0xff;
            au+=u;
            av+=v;

            if(j%2==0){
                *(chroma_p+j)=(au>>2)&0xff;
                *(chroma_p+j+1)=(av>>2)&0xff;
                av=au=0;
            }

        }
    }
}


/*
void load_rgb_bgrx(unsigned char*yuv,unsigned char*rgb,
        int left,int top,int width,int height,//patch rectangle
        int rgbheight,
        int ostride)
*/

void load_rgb_bgrx(
        unsigned char*bgrx,
        unsigned char*nv12, 
        int pleft,int ptop,int pwidth,int pheight,//rgb patch rect
        int width,int height,//rgb data size
        int sstride,
        int dstride //yuv data stride<pixel>
        )

{
    //assert left top width height are even;
    //

    if (sstride == 0)
        sstride = width;

    if (dstride == 0)
        dstride = width;

    int luma_off=dstride*height;

    unsigned char*luma_p0,*luma_p1;
    unsigned char*chroma_p;
    unsigned char*rgb_p0,*rgb_p1;
   
    int au;//(u1+u2+u3+u4)/4
    int av;//

    unsigned char r,g,b;
    unsigned char y,u,v;
//    fprintf(stderr,"LOAD {x:%d, y:%d, w:%d, h:%d, ww:%d, hh:%d }\n",left,top,width,height,stride,vstride);

    int i,j;
    for(i=ptop;i<pheight+ptop;i+=2){
        //rows
        rgb_p0=bgrx+sstride*(i)*4;
        rgb_p1=bgrx+sstride*(i+1)*4;
        luma_p0=nv12+dstride*i;
        luma_p1=nv12+dstride*(i+1);
        chroma_p=nv12+luma_off+dstride*(i/2);

        for(j=pleft;j<pwidth+pleft;j++){

            b=*(rgb_p0+j*4);
            g=*(rgb_p0+j*4+1);
            r=*(rgb_p0+j*4+2);

            rgb2yuv_pixel(r,g,b,&y,&u,&v);
            *(luma_p0+j)=(char)y&0xff;
            au=u;
//            av=v;
///////////
            b=*(rgb_p1+j*4);
            g=*(rgb_p1+j*4+1);
            r=*(rgb_p1+j*4+2);

            rgb2yuv_pixel(r,g,b,&y,&u,&v);
            *(luma_p1+j)=(char)y&0xff;
//            au+=u;
            av=v;

            if(j%2==0){
                *(chroma_p+j)=au&0xff;
                *(chroma_p+j+1)=av&0xff;
//                av=au=0;
            }
        }
    }
}




#if 0
void load_rgb_bgrx__(
        unsigned char*bgrx,
        unsigned char*nv12, 
        int pleft,int ptop,int pwidth,int pheight,//rgb patch rect
        int width,int height,//rgb data size
        int sstride,
        int dstride //yuv data stride<pixel>
        )
{
   
    unsigned char*luma_p=nv12;
    unsigned char*chroma_p;

    unsigned char*rgb_p=bgrx;

    if (sstride == 0)
        sstride = width;

    if (dstride == 0)
        dstride = width;

    chroma_p=luma_p+dstride*height;

    unsigned char b,g,r;
    unsigned char y,u,v;

    int i,j;

    for(i=ptop;i<pheight;i+=2){//vertical
//==============
    rgb_p=bgrx+i*sstride*4;
    luma_p=nv12+dstride*i;
    chroma_p=nv12+dstride+height+dstride*(i/2);
        for(j=pleft;j<pwidth+pleft;j++){

            b=*(rgb_p+j*4);
            g=*(rgb_p+j*4+1);
            r=*(rgb_p+j*4+2);

            y= 0.299*r + 0.587*g + 0.114*b;
            *(luma_p+j)=(char)y&0xff;

//            if(j%2==0){
                u= -0.169*r - 0.331*g + 0.5*b+128;
                *(chroma_p+j)=(char)u&0xff;
//            }
        }
    //odd line
        rgb_p+=sstride*4;
        luma_p+=dstride;
         for(j=pleft;j<pwidth+pleft;j++){

            b=*(rgb_p+j*4);
            g=*(rgb_p+j*4+1);
            r=*(rgb_p+j*4+2);

            y= 0.299*r + 0.587*g + 0.114*b;
            *(luma_p+j)=(char)y&0xff;
          
//            if(j%2==0){
                v= 0.5*r - 0.419*g - 0.081*b+128;
                *(chroma_p+j+1)=(char)v&0xff;
//            } 
        }

//    }


    }





}


#endif



void load_yuv_yv12(unsigned char*yv12,unsigned char*nv12,int width,int height,int sstride,int dstride)
{

    unsigned char*nv12_luma=nv12;
    unsigned char*nv12_chroma;

    unsigned char*yv12_luma=yv12;
    unsigned char*yv12_v;
    unsigned char*yv12_u;



    if (sstride == 0)
        sstride = width;

    if (dstride == 0)
        dstride = width;

    nv12_chroma=nv12_luma+dstride*height;
    
    yv12_v=yv12_luma+sstride*height;
    yv12_u=yv12_v+sstride*height/4;


    int y;
    int x;

    for (y = 0 ; y < height ; y++){
        memcpy(nv12_luma + (dstride*y), yv12_luma + (sstride*y) , width);
    }

    for (y = 0 ; y < height/2 ; y++){
        for (x= 0 ; x < width; x=x+2){
            nv12_chroma[(y*dstride) + x] =    yv12_v[((sstride/2)*y) + (x >>1)];
            nv12_chroma[(y*dstride) +(x+1)] = yv12_u[((sstride/2)*y) + (x >>1)];
        }
    }
   

}


void load_yuv_nv12(unsigned char*inyuv, unsigned char*outyuv,int width,int height,int istride,int ostride)
{

    if(istride==0)
        istride=width;
    if(ostride==0)
        ostride=width;


    unsigned char*inyuv_chroma=inyuv+width*istride;
    unsigned char*outyuv_chroma=outyuv+width*ostride;

    int y;

    for(y=0;y<height;y++){
        memcpy(outyuv+y*ostride,inyuv+y*istride,width);
    }

    for(y=0;y<height/2;y++){
        memcpy(outyuv_chroma+y*ostride/2,inyuv_chroma+y*istride/2,width/2);
    }

}



}//extern "C"





