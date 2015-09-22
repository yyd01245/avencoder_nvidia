#include"utils.h"


int get_id(int range)
{
//id [0,range)
    
#define BSIZ 5

    char idbuf[BSIZ]="";
    int cur_id=0;
    int ret_id;
    int nr,nw;

    int fd=open("idfile",O_CREAT|O_RDWR,0666);
    if(fd<0){
        fprintf(stderr,"Can not open idfile\n");
        cur_id=0;
//        return 0;
    }else{
        
        nr=read(fd,idbuf,5);
        if(nr<=0){
            cur_id=0;
        }else{
            cur_id=atoi(idbuf);
            if(cur_id<0)
                cur_id=0;
        }
    }
   
    ret_id=cur_id%range;
//GET CURRENT ID;
//
//SET NEXT ID to file
    int next_id;
    next_id=(cur_id+1)%range;

    (void)snprintf(idbuf,5,"%d\n",next_id);
    
    (void)lseek(fd,SEEK_SET,0);

    nw=write(fd,idbuf,5);
    if(nw<=0){
        fprintf(stderr,"ERROR:Write New Value\n");
    }

    close(fd);

    return ret_id;
}






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


void print_gpu_load(loadavg*l)
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

int get_gpu_loadavg(loadavg*l)
{
/*
 * nvidia-smi --query-compute-apps=gpu_bus_id,name,pid, --format=csv,noheader
 *
 */


    if(!l){
        fprintf(stderr,"Skipping...\n");
        return -1;
    }

    l->nb_cores=NB_ID;
//    l->gvals=(unsigned short*)calloc(l->nb_cores,sizeof(unsigned short));

//    char (*lbuff)[LINESIZ]=(char(*)[LINESIZ])calloc(MAXLLEN,LINESIZ);

    char lbuff[LINESIZ];
    bzero(lbuff,sizeof(lbuff));
//    char*lbuff=(char*)calloc(1,LINESIZ);

    FILE*pfp=popen(CMD,"r");
    if(!pfp){
        fprintf(stderr,"\033[33mExecuting CMD[%s] Failed{%s}!!\033[0m\n",CMD,strerror(errno));
        return -2;
    }

//        fprintf(stderr,"\033[33mExecuting CMD[%s] Success{--}!!\033[0m\n",CMD);
//process data
    int nl=0;
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


/*
int main(){



    int nid;
    while(1){
        nid=get_next_gpuid();

        printf("Next GPUID :%d\n",nid);

        sleep(1);
    }
    return 0;
}
*/
