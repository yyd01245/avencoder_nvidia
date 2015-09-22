#include<stdio.h>
#include<string.h>
#include<nvidia/gdk/nvml.h>




#define NDEV 12
#define MAXPROC 20



static int Byte2Mib(unsigned int byte)
{

    return byte/(1024*1024);
}




static int get_mem_info(unsigned int*ncores,unsigned int*usedarray)
{

    nvmlReturn_t ret;
    ret=nvmlInit();


    if(ret!=NVML_SUCCESS){
        fprintf(stderr,"ERROR:: Initialize NVML{%s}..\n",nvmlErrorString(ret));
        return -1;
    }


    unsigned int c;

    ret=nvmlDeviceGetCount(&c);
    if(ret!=NVML_SUCCESS){
        fprintf(stderr,"ERROR:: Device Get Count{%s}..\n",nvmlErrorString(ret));
        return -1;
    }
    
    *ncores=c;

    nvmlDevice_t devs[NDEV];
    nvmlMemory_t meminfo;


    int i;
    for(i=0;i<c;i++){
        
        ret=nvmlDeviceGetHandleByIndex(i,&devs[i]);
        if(ret!=NVML_SUCCESS){
            fprintf(stderr,"ERROR:: Device Get Handle{%s}..\n",nvmlErrorString(ret));
            return -1;
        }

        ret=nvmlDeviceGetMemoryInfo(devs[i],&meminfo);
        if(ret!=NVML_SUCCESS){
            fprintf(stderr,"ERROR:: GetMemoryInfo{%s}..\n",nvmlErrorString(ret));
            return -1;
        }
        usedarray[i]=meminfo.used;

    }

    ret=nvmlShutdown();

    if(ret!=NVML_SUCCESS){
        fprintf(stderr,"ERROR:: Shutdown NVML{%s}..\n",nvmlErrorString(ret));
        return -1;
    }

    return 0;

}



static int get_process_info(unsigned int*ncores,unsigned int *valarray)
{

    nvmlReturn_t ret;

    ret=nvmlInit();

    if(ret!=NVML_SUCCESS){
        fprintf(stderr,"ERROR:: Initialize NVML{%s}..\n",nvmlErrorString(ret));
        return -1;
    }


    unsigned int c;

    ret=nvmlDeviceGetCount(&c);
    if(ret!=NVML_SUCCESS){
        fprintf(stderr,"ERROR:: Device Get Count{%s}..\n",nvmlErrorString(ret));
        return -1;
    }
    
    *ncores=c;
/*
    if(c!=NDEV){
        fprintf(stderr,"ERROR:: Current number of Cores is [%d],not %d....YOU NEED RECOMPILE THIS ROUTINE\n",c,NDEV);
        return -2;
    }
*/
    nvmlDevice_t devs[NDEV];

    nvmlProcessInfo_t pis[MAXPROC];


    int i;
    for(i=0;i<c;i++){
        
        ret=nvmlDeviceGetHandleByIndex(i,&devs[i]);
        if(ret!=NVML_SUCCESS){
            fprintf(stderr,"ERROR:: Device Get Handle{%s}..\n",nvmlErrorString(ret));
            return -1;
        }

        unsigned int np=MAXPROC;
        ret=nvmlDeviceGetComputeRunningProcesses(devs[i],&np,pis);
        if(ret!=NVML_SUCCESS){
            fprintf(stderr,"ERROR:: GetRunningProcess{%s}..\n",nvmlErrorString(ret));
            return -1;
        }
        valarray[i]=np;

    }

    ret=nvmlShutdown();

    if(ret!=NVML_SUCCESS){
        fprintf(stderr,"ERROR:: Shutdown NVML{%s}..\n",nvmlErrorString(ret));
        return -1;
    }

    return 0;


}



int get_gpu_id_mem()
{
    unsigned int ncores;
    unsigned int gpuarray[NDEV];

    int ret=get_mem_info(&ncores,gpuarray);
    if(ret<0){
        fprintf(stderr,"Can not Get Next GPU ID Correctly..\n");
        return 0;
    }

    int i;
    int ind;
    int val=(1L<<31)-1;
    fprintf(stderr,"\033[33mVal:=%d\n\033[0m",val);
    for(i=0;i<ncores;i++){
        printf("[%d]::%d(%d)\n",i,Byte2Mib(gpuarray[i]),gpuarray[i]);
        if(gpuarray[i]<val){
            val=gpuarray[i];
            ind=i;
        }
    }

    return ind;
}






int get_gpu_id_proc()
{
    unsigned int ncores;
    unsigned int gpuarray[NDEV];

    int ret=get_process_info(&ncores,gpuarray);
    if(ret<0){
        fprintf(stderr,"Can not Get Next GPU ID Correctly..\n");
        return 0;
    }

    int i;
    int ind;
    int val=1000;
    for(i=0;i<ncores;i++){
        if(gpuarray[i]<val){
            val=gpuarray[i];
            ind=i;
        }
    }

    return ind;
}






#if 0

#define BUSID(id) "0000:" id ":00.0"

struct _bus2id{

    char bus[16];
    int id;
}bus2id[]={
    {BUSID("06"),0},
    {BUSID("07"),1},
    {BUSID("08"),2},
    {BUSID("09"),3},
    {BUSID("85"),4},
    {BUSID("86"),5},
    {BUSID("87"),6},
    {BUSID("88"),7},
    {"END",-1}
};

#define NB_ID (sizeof(bus2id)/sizeof(bus2id[0])-1)



//     0000:87:00.0, 6286

char*id_to_bus(int id){

    int i;
    for(i=0;bus2id[i].id!=-1;i++){
        if(bus2id[i].id==id)
            break;
    }
    if(bus2id[i].id==-1)
        return NULL;

    return bus2id[i].bus;

}


int bus_to_id(const char*bus){
    
//    fprintf(stderr,"BUS<%s> to ID\n",bus);
    int i;
    for(i=0;bus2id[i].id!=-1;i++){
//        fprintf(stderr,"\033[32m[%s]::[%s]\033[0m\n",bus,bus2id[i].bus) ;
        if(!strncmp(bus2id[i].bus,bus,10))
            break;
    }
    if(bus2id[i].id==-1){
        fprintf(stderr,"Can not find id correspond to [%s]\n",bus);
        return -1;
    }

    return i;
}



typedef struct _gpuload{

    int nb_cores;
    unsigned short gvals[8];

}gpuload;








#if 0
void print_gpu_load(gpuload*l)
{

    int i;
    for(i=0;i<NB_ID;i++){
        printf(" %d::[%d]\n",i,l->gvals[i]);
    }

}


#define LINESIZ 128
#define MAXLLEN 52

#define CMD "nvidia-smi --query-compute-apps=gpu_bus_id,pid, --format=csv,noheader"

//#define CMD "./cmd"

int get_gpu_load(gpuload*l)
{


    while(fgets(lbuff,128,pfp)){
//        fprintf(stderr,"\n\033[32m=>>%s<<\033[0m\n",lbuff[nl]);
        int gid=bus_to_id(lbuff);
        if(gid<0){
            fprintf(stderr,"ERROR: Get bus id \n");
            continue;
        }
        nl++;
        l->gvals[gid]+=1;
    }

//    fprintf(stderr,"\033[33mREAD %d Lines\033[0m\n",nl);

    pclose(pfp);

//    free(lbuff);
//    free(l->gvals);

    return nl;
}




static int get_fmin_index(loadavg*l)
{

    int i;
    int index;
    int min=100000;//large enough
    for(i=0;i<l->nb_cores;i++){
        if(l->gvals[i]<min){
            min=l->gvals[i];
            index=i;
        }
    }
    
    return index;
}


int get_next_gpuid()
{
    int current_total_threads=0;
    int ret;
    loadavg avg;
    memset(&avg,0,sizeof(avg));

//    fprintf(stderr,"\033[34m NB_ID: %d \033[0m\n",NB_ID);

    ret=get_gpu_loadavg(&avg);
    
    print_gpu_load(&avg);

    if(ret<0){
        fprintf(stderr,"ERROR: get_next_gpuid()!!\n");
        return -1;
    }

    current_total_threads=ret;
/*
    if(current_total_threads==last_total_threads){
        fprintf(stderr,"Error Detected,Can not assign more tasks on GPU..\n");
  //      return -3;
    }
    last_total_threads=current_total_threads;
*/
    int index=get_fmin_index(&avg);
    if(index<0){
        //never fall in
        fprintf(stderr,"Due to the GPU's limit<..> ,Can not add more tasks!!\n");
        return -2;
    }

   /* 
    /////for TEST
    /////
    int fd=open("ffff",O_WRONLY|O_APPEND|O_CREAT,0666);
    char buff[128];
    snprintf(buff,128,"%s\n",id_to_bus(index));
    int blen=strlen(buff);
    write(fd,buff,blen);
    close(fd);
    */
    fprintf(stderr,"Current GPU Tasks[%d] \n",current_total_threads);
    return index;

}


#endif



#endif


#if 0
int main(){



    int n;
    int arr[10];

    get_process_info(&n,arr);

    int i;
    for(i=0;i<n;i++){
        printf("%d::%d\n",i,arr[i]);
    }

    printf("=====>[%d]\n",get_gpu_id());

#if 0
    int nid;
    while(1){
        nid=get_next_gpuid();

        printf("Next GPUID :%d\n",nid);

        sleep(1);
    }
#endif

    return 0;
}

#endif 

