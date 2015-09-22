
#include"confile.h"

#include<stdlib.h>
#include<string.h>
#include<stdio.h>



#define SEP '.'
#define SEPARATOR_S "."
#define COMMENT_START_S "#"


static cf_node_t* _confile_node_new(const char*key,const char*value)
{

    cf_node_t*node;

    node=(cf_node_t*)calloc(1,sizeof(cf_node_t));
//    node->next=NULL;
    if(!node)
        return NULL;
    if(key)
        node->cfn_key=strdup(key);

    if(value)
        node->cfn_val=strdup(value);

    return node;
}


static void _confile_node_del(cf_node_t*node)
{

    if(!node)
        return;

    if(node->cfn_key)
        free(node->cfn_key);

    if(node->cfn_val)
        free(node->cfn_val);

    free(node);

}



static void _confile_node_add_sub_node(cf_node_t*parent,cf_node_t*node)
{
    if(!parent||!node)
        return;

    //node->prev=NULL;//ASSERTED
    node->next=NULL;//ASSERTED
    cf_node_t*cur=parent->p_child;

    if(!cur){//no child yet 
        parent->p_child=node;
    }else{
        while(cur->next){
            cur=cur->next;
        }
        cur->next=node;
    }

}

static cf_node_t* confile_node_add_sub_key(cf_node_t*parent,const char*key,const char*value)
{
    cf_node_t*newnode=_confile_node_new(key,value);
    _confile_node_add_sub_node(parent,newnode);
    return newnode;
}





static int str_cmp(const char*str0,const char*str1,int nocase)
{
    if(!str0 ||!str1)
        return 0;

    if(nocase)
        return !strcasecmp(str0,str1);
    else
        return !strcmp(str0,str1);

}



static cf_node_t*_confile_node_find_sub_key(cf_node_t*parent,const char*key,int nocase)
{

    cf_node_t*ret_node=NULL;
    if(!parent)
        return NULL;
cf_node_t*cur=parent->p_child;

    while(cur){
        if(str_cmp(key,cur->cfn_key,nocase)){
            ret_node=cur;
        }
        cur=cur->next;
    }

    return ret_node;

}



static char**get_fields_( char*conf_item, const char*sep,int*osiz)
{
    int sz_fields=2;
    char**ofields=(char**)calloc(sz_fields,sizeof(char*));

    char* savptr,*strit;
    char*tok;
    int ind=0;
    for(strit=(char*)conf_item;;strit=NULL,ind++){

        tok=strtok_r(strit,sep,&savptr);
        if(NULL==tok)
            break;
        ofields[ind]=tok;
//        fprintf(stderr,"-(%d):[%s]\n",ind,ofields[ind]);
        if(ind+1>=sz_fields){
            sz_fields*=2;
            ofields=(char**)realloc(ofields,sz_fields*sizeof(char*));
        }

    }

    ofields[ind]=NULL;

    if(osiz)
        *osiz=ind;

    return ofields;

}

static char**get_fields( char*conf_item, const char*sep)
{
    int sz_fields=2;
    char**ofields=(char**)calloc(sz_fields,sizeof(char*));

    char* savptr,*strit;
    char*tok;
    int ind=0;
    for(strit=(char*)conf_item;;strit=NULL,ind++){

        tok=strtok_r(strit,sep,&savptr);
        if(NULL==tok)
            break;
        ofields[ind]=strdup(tok);
//        fprintf(stderr,"-(%d):[%s]\n",ind,ofields[ind]);
        if(ind+1>=sz_fields){
            sz_fields*=2;
            ofields=(char**)realloc(ofields,sz_fields*sizeof(char*));
        }

    }

    ofields[ind]=NULL;
//    *osiz=ind;

    return ofields;
}



void free_fields_(char**fields)
{
    if(fields)
        free(fields);
    
}



void free_field(char**fields)
{
    int i;
    for(i=0;fields[i];i++)
        free((char*)fields[i]);

    free(fields);
    
}



static cf_node_t* _confile_build_configs(cf_node_t*root,char**fields)
{

    if(!*fields){//*field == NULL 
        return root;
    }

    cf_node_t*newroot=_confile_node_find_sub_key(root,*fields,1);

    if(!newroot){//not found
        newroot=confile_node_add_sub_key(root,*fields,NULL);
    }

    return _confile_build_configs(newroot,++fields);

}



static char* _strip_line(char*line)
{

    char*p=line;

    if(!line)
        return 0;

    int len=strlen(line);

        
    while(*p==' '||*p=='\n'||*p=='\t'||*p=='\r')
        p++;

    char*rp=p;
    while(*rp!=' '&&*rp!='\n'&&*rp!='\t'&&*rp!='\0'&&*rp!='\r')
        rp++;

    *rp='\0';

    return p;

}


static int _parse_line(char*line,char**key,char**value)
{

    char*pcomment;
    pcomment=strstr(line,COMMENT_START_S);
    if(pcomment)
        *pcomment='\0';
    
    char*strit=line;
    char*savptr,*tok;

    int it=0;

    for(;;strit=NULL,it++){

        tok=strtok_r(strit,"=",&savptr);
        if(tok){
            if(0==it) *key=_strip_line(tok);
            else if(1==it) *value=_strip_line(tok);
        }else{
            break;
        }

    }

//    fprintf(stderr,"KEY[%s]:VALUE{%s}\n",*key,*value);

    //FIXME:only parse the line consists of `<key>=<val>'
    return 2==it;

}


confile_t* confile_parse_file(const char*filename)
{

    confile_t*pconf=confile_new(filename);
    cf_node_t*pnode;

    char*key,*value;
    int nit;

    FILE*fp=fopen(filename,"r");
    if(!fp){
//        fprintf(stderr,"0000\n");
        return NULL;
    }

    int lsiz=512*sizeof(char);
    char*line=(char*)malloc(lsiz);
    bzero(line,lsiz);

    while(fgets(line,lsiz,fp)){
        
        nit=_parse_line(line,&key,&value);
        if(!nit)
            continue;

        pnode=confile_set_config(pconf,key,value);

    }

    fclose(fp);
    free(line);

    return pconf;
}


boolean confile_save_file(confile_t*pconf)
{
    
    if(!pconf||!pconf->cf_filename)
        return FALSE;

    FILE*fp=fopen(pconf->cf_filename,"w");
    confile_string(pconf,fp);

    fclose(fp);

    return TRUE;

}



static cf_node_t* _confile_node_find_key_R(cf_node_t*root,const char**fields)
{

    cf_node_t*cur=root->p_child;

    while(cur){
//        printf(":::::-----CMP{%s,%s}\n",cur->cfn_key,*fields);
        if(str_cmp(cur->cfn_key,*fields,1)){
//            printf("++:%s:\n",cur->cfn_key);
            ++fields;
            if(!*fields){
//                printf("[%s]\n",*fields);
                return cur;
            }else{
//                printf(">[%s]\n",*fields);
                return _confile_node_find_key_R(cur,fields);
            }
        }
        cur=cur->next;
    }
    return NULL;//not found

}


cf_node_t* _confile_node_find_key_R_2(cf_node_t*root,const char**fields,cf_node_t**parent,cf_node_t**pret)
{

    cf_node_t*cur=root->p_child;
    *parent=root;
    *pret=cur;

    while(cur){
//        printf(":::::-----CMP{%s,%s}\n",cur->cfn_key,*fields);
        if(str_cmp(cur->cfn_key,*fields,1)){
//            printf("++:%s:\n",cur->cfn_key);
            ++fields;
            if(!*fields){
//                printf("[%s]\n",*fields);
                return cur;
            }else{
//                printf(">[%s]\n",*fields);
                return _confile_node_find_key_R_2(cur,fields,parent,pret);
            }
        }
        cur=cur->next;
    }
    return NULL;//not found

}






static void _confile_node_remove_sub_nodes_R(cf_node_t*root)
{

    cf_node_t*cur=root->p_child;

    while(cur){
        _confile_node_remove_sub_nodes_R(cur);
        _confile_node_del(cur);
        cur=cur->next;
    }
   
    root->p_child=NULL;

}





boolean confile_remove_config(confile_t*confile,const char*fullkeys)
{
    int siz;
    
    char*line=strdup(fullkeys);
    char**fields=get_fields_(line,SEPARATOR_S,&siz);

    cf_node_t*parent;
    cf_node_t*obj;
    obj=_confile_node_find_key_R_2(confile->cf_root,(const char**)fields,&parent,&obj);
//    root=_confile_node_find_key_R_2(cf_node_t*root,const char**fields,cf_node_t**parent,cf_node_t**pret)
    
    free_fields_(fields);
    free(line);

    if(obj){
        _confile_node_remove_sub_nodes_R(obj);
//        _confile_node_del(obj);

        cf_node_t*cur=parent->p_child;
        if(cur==obj){
//            tmp=cur;
            parent->p_child=cur->next;
        }else{
            while(cur->next!=obj){
                cur=cur->next;
            }
            //ASSERT::cur->next!=NULL
//            if(cur->next)
              cur->next=obj->next;
        }
        _confile_node_del(obj);
        return TRUE;
    }

    return FALSE;
}






cf_node_t* confile_set_config(confile_t*confile,const char*fullkeys,const char*value)
{
    char*line=strdup(fullkeys);
    
    cf_node_t*root,*pnode;
    char**fields;

    root=confile->cf_root;
    fields=get_fields_(line,SEPARATOR_S,NULL);
    pnode=_confile_build_configs(root,(char**)fields);
    free_fields_(fields);

    
    if(pnode->cfn_val){
        free(pnode->cfn_val);
        pnode->cfn_val=NULL;
    }
    if(value)
        pnode->cfn_val=strdup(value);

    free(line);

    return pnode;

}


cf_node_t* confile_get_config(confile_t*confile,const char*fullkeys,char**value)
{

    char*line=strdup(fullkeys);
    
    cf_node_t*root,*pnode;
    char**fields;

    root=confile->cf_root;
    fields=get_fields_(line,SEPARATOR_S,NULL);
 

    pnode=_confile_node_find_key_R(confile->cf_root,(const char**)fields);

    free_fields_(fields);
    free(line);

    if(value){
        if(pnode)
            *value=strdup(pnode->cfn_val);
        else
            *value=NULL;
    }
    return pnode;

}


void str_fields(char**fields,FILE*fp)
{
    int i=1;
    fprintf(fp,"%s",fields[i]);
    for(i=2;fields[i];i++){
        fprintf(fp,SEPARATOR_S"%s",fields[i]);
    }
}


typedef struct user_data{

    FILE*fp;
    char*value;
}user_data;


typedef void _callback(cf_node_t*cur,char**fieldpath,void*userdata);


static void str_item(cf_node_t*node,char**fieldpath,void*userdata)
{
//    struct user_data*ud=(struct user_data*)userdata;
    str_fields((char**)fieldpath,stderr);
    fprintf(stderr,":\n");
}

void _confile_traverse_leaf(cf_node_t*root,char**fieldpath,int pos,_callback cb,void*userdata)
{

    struct user_data*ud=(struct user_data*)userdata;

    cf_node_t*cur=root->p_child;

    fieldpath[pos]=root->cfn_key;
    if(!cur){
        fieldpath[pos+1]=0;
        ud->value=root->cfn_val;
//        str_fields(fieldpath);
        cb(root,fieldpath,userdata);
        return;
    }

    while(cur){
        _confile_traverse_leaf(cur,fieldpath,pos+1,cb,userdata);
        cur=cur->next;
    }

}

static void out_item(cf_node_t*node,char**fieldpath,void*userdata)
{

    struct user_data*ud=(struct user_data*)userdata;

    FILE*ofp=ud->fp;
    str_fields((char**)fieldpath,ofp);
    fprintf(ofp," = %s\n",ud->value);
}



void confile_string(confile_t*confile,FILE*ofp)
{

    user_data d;
    d.value=NULL;
    d.fp=ofp;

    char**fields=(char**)calloc(32,sizeof(char*));

    _confile_traverse_leaf(confile->cf_root,fields,0,out_item,&d);
    free(fields);

}





confile_t* confile_new(const char*filename)
{

    confile_t*p_confile=(confile_t*)calloc(1,sizeof(confile_t));


    if(filename)
        p_confile->cf_filename=strdup(filename);
    else{
        fprintf(stderr,"WARN::Filename is NULL\n");
        return NULL;
    }
    p_confile->cf_root=(cf_node_t*)calloc(1,sizeof(cf_node_t));

    p_confile->cf_root->cfn_key=strdup("");

    return p_confile;
}



void confile_free(confile_t*pconf)
{
    
    if(pconf)
        _confile_node_remove_sub_nodes_R(pconf->cf_root);

    //remove root node
    _confile_node_del(pconf->cf_root);

    if(pconf->cf_filename)
        free(pconf->cf_filename);

    free(pconf);
}





void print_indent(int i)
{
    while(i--){
        printf("\t");
    }
}



void print_tree(cf_node_t*root)
{
    static int indent=0;

    if(!root)
        return;

    print_indent(indent);

    if(root){

        printf("->%s",root->cfn_key);
        if(root->cfn_val)
            printf(":[%s]",root->cfn_val);
        printf("\n");
    }

    cf_node_t*cur=root->p_child;

    indent++;
    while(cur){
        print_tree(cur);
        cur=cur->next;
    }
    indent--;

}


static void confile_print(confile_t*pconf)
{
    cf_node_t*node;
    
    printf("===============\n");
    if(pconf)
        node=pconf->cf_root;

    print_tree(node);
    printf("---------------\n");

}




static void print_fields(const char**fields)
{
    int i=0;
    for(i=0;fields[i];i++){
        fprintf(stderr,"(%d):[%s]\n",i,fields[i]);
    }

}

#if 0

int main(int argc,char**argv)
{
    
    int sz=0;

//    fprintf(stderr,"====%d====\n",sz);
//    print_fields((const char**)fields);

    confile_t*pconf=confile_new("OOOOO");

    cf_node_t* pnode;
    cf_node_t* node;

//    cf_node_t node;
//    bzero(&node,sizeof(node));
    fprintf(stderr,"========\n");

    
//    char**fields=get_fields(argv[2],argv[1]);
//    _confile_build_configs(&node,(const char**)fields);
//    free(fields);
  
    pnode=confile_set_config(pconf,argv[2],"XXX");


//    fields=get_fields(argv[3],argv[1]);
//    _confile_build_configs(&node,(const char**)fields);
//    free(fields);
    pnode=confile_set_config(pconf,argv[3],"YYY");


//    fields=get_fields(argv[4],argv[1]);
//    pnode=_confile_build_configs(&node,(const char**)fields);
//    free(fields);
    pnode=confile_set_config(pconf,argv[4],"ZZZ");
    pnode->cfn_val="hello,world";

    printf("=%p=\n",pconf->cf_root);

//    print_tree(pconf->cf_root);

    confile_print(pconf);

    char*target=strdup(argv[3]);

    char**fields=get_fields(target,SEPARATOR_S);
    print_fields((const char**)fields);
    printf("000000000000000000000\n");

    pnode=_confile_node_find_key_R(pconf->cf_root,(const char**)fields);
  
    printf("=-=-=-{%s}\n",pnode->cfn_val);
    pnode->cfn_val="HHHHH";

    confile_print(pconf);

    free_field((const char**)fields);
    free(target);

    printf("000000000000000000000\n");
//    confile_remove_sub_nodes_at(pnode);
    
    confile_free(pconf);
    confile_print(pconf);

    return EXIT_SUCCESS;
}

#elif 0



int main(int argc,char**argv)
{
    
    int sz=0;


    confile_t*pconf=confile_new("OOOOO");

    cf_node_t* pnode;

    fprintf(stderr,"========\n");

    char confitemp[100]="AA-BB-CC";
  
    pnode=confile_set_config(pconf,confitemp,"X");
    pnode=confile_set_config(pconf,"1-2-3-6-7-9","Z");
    pnode=confile_set_config(pconf,"LOL","Y");
    pnode=confile_set_config(pconf,"LOL-00","zoer");
    pnode=confile_set_config(pconf,"AA-123-helo","raa");
    pnode=confile_set_config(pconf,"1-2-3-4","Y");

    confile_print(pconf);
    fprintf(stderr,"========\n");

    FILE*ofp=fopen("conf0.conf","w");
    confile_print(pconf);
    fprintf(stderr,"-+-+-+-+-+-+-=\n");
    confile_string(pconf,ofp);
//    pnode=confile_set_config(pconf,"11-22","ZZ");
    fprintf(stderr,"-+-+-+-+-+-+-=\n");


 
    if(argc>1)
        confile_remove_config(pconf,argv[1]);
    else
        confile_remove_config(pconf,"AA");

   
    fclose(ofp);
    ofp=fopen("conf1.conf","w");
    confile_string(pconf,ofp);
    confile_print(pconf);

    char*p;

    confile_get_config(pconf,"AA-BB-CC",&p);


    fclose(ofp);

    fprintf(stderr,"====[%s]====\n",p);
    free(p);

    confile_free(pconf);
    confile_print(pconf);


    return EXIT_SUCCESS;


}

#elif 0


int main(int argc,char**argv)
{
    


    confile_t*pconf=confile_parse_file(argv[1]);


    confile_print(pconf);


    char*p;

    cf_node_t*pnode;

    pnode=confile_get_config(pconf,argv[2],&p);

    printf("=[%s]=\n",p);

    if(!pnode)
        printf("xxxxxxxxxx\n");
    else
        printf("(%s:%s)\n",pnode->cfn_key,pnode->cfn_val);


    return EXIT_SUCCESS;
}



#endif




