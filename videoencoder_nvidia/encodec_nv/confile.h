
#ifndef _H_CONFILE_
#define _H_CONFILE_

#include<stdio.h>




typedef int boolean;

#define FALSE 0
#define TRUE (!FALSE)



typedef struct _conf_node{

    char* cfn_key;
    char* cfn_val;

    struct _conf_node* p_child;
    struct _conf_node* next;

}cf_node_t;





typedef struct _confile{

    char*cf_filename;

    cf_node_t* cf_root;

}confile_t;





confile_t* confile_new(const char*filename);


boolean confile_save_file(confile_t*);


cf_node_t* confile_set_config(confile_t*confile,const char*fullkey,const char*value);


cf_node_t* confile_get_config(confile_t*confile,const char*fullkey,char**value);


boolean confile_remove_config(confile_t*confile,const char*fullkeys);


confile_t* confile_parse_file(const char*filename);


void confile_string(confile_t*confile,FILE*ofp);


void confile_free(confile_t*);














#endif
