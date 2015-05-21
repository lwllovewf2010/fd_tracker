#include <cutils/hashmap.h>
#include<openssl/md5.h>
#include <stdio.h>

int str_hash(void *key) {
    return hashmapHash(key, strlen((char*)key));
}

bool str_equals(void *key_a, void *key_b) {
    return strcmp((char*)key_a, (char*)key_b) == 0;
}

#define MD_SIZE 16

char* md5 (char * data, char * data2) {
    MD5_CTX ctx;
    unsigned char md[MD_SIZE] = {0};

    MD5_Init(&ctx);
    MD5_Update(&ctx,data,strlen(data));
    MD5_Update(&ctx,data2,strlen(data2));
    MD5_Final(md,&ctx);

    char* ret = (char*) malloc(MD_SIZE*2+1);
    bzero(ret, MD_SIZE*2+1);

    char tmp[3]={0};
    for(int i=0; i<16; i++ ){
        sprintf(tmp,"%02X",md[i]);
        strcat(ret,tmp);
    }
    return ret;
}
