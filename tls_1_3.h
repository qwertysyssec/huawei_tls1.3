
#ifndef UUID12AA4FE9_1E70_4EF5_806E_58121E2FC99F
#define UUID12AA4FE9_1E70_4EF5_806E_58121E2FC99F

#define __STDC_FORMAT_MACROS
#include <inttypes.h>        /* sprintf formating */
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <mqueue.h>          /* mq*/
#include <semaphore.h>       /* sem_t +compiler flag*/
#include <stdint.h>          /* uint64_t */
#include <sys/mman.h>        /* mmap */
#include <unistd.h>          /* close */
#include <stdlib.h>          /* exit */
#include <stddef.h>          /* NULL*/
#include <string.h>          /* strcat */
#include <stdio.h>           /* sprintf*/
#include <pthread.h>         /* pthreads mutexes +compiler flag*/
#include <unistd.h>          /* ftruncate */
#include <sys/types.h>       /* off_t */
#include <signal.h>          /* try make async safe */
#include <stddef.h>          /* size_t */
#include <string.h>          /* strcpy */
#include <stdbool.h>         /* false/true */
#include <errno.h>           /* error handling*/

static const char* ENTRANCE_KEY="/TLS_DAEMON_ENTRANCE_UUIDDA85B873_13CB_417A_BF11_40B249701BA7";
static const char* UNICITY_GEN_KEY="/TLS_DAEMON_UNICITY_GEN_UUIDDA85B873_13CB_417A_BF11_40B249701BA7";
static const char* PREFIX="/TLS_DAEMON_OPER_ID_";
static const char* POSTFIX="_UUIDDA85B873_13CB_417A_BF11_40B249701BA7";
static const unsigned char OPEN=0;
static const unsigned char CLOSE=1;
static const unsigned char READ=2;
static const unsigned char WRITE=3;

typedef int tls_connection_t;

typedef struct {
    unsigned char type;
    uint64_t oper_id;
    tls_connection_t connec_id;
}message_t;



typedef struct{
    pthread_mutex_t mutex;
    uint64_t counter;
}counter_t;

typedef struct{
    int shm_fd;
    counter_t* addr;
}unicity_gen_t;

typedef struct{
    mqd_t mqd;
    unicity_gen_t unicity_gen;
}tls_context_t;

void tls_init(tls_context_t * context){
     if((context->mqd=mq_open(ENTRANCE_KEY, O_WRONLY))==(mqd_t) -1) exit(EXIT_FAILURE);
     if((context->unicity_gen.shm_fd=shm_open(UNICITY_GEN_KEY, O_RDWR, 0))==-1) exit(EXIT_FAILURE);
     if((context->unicity_gen.addr=mmap(NULL,sizeof(counter_t),PROT_READ|PROT_WRITE,MAP_SHARED,
                                        context->unicity_gen.shm_fd, 0))==MAP_FAILED)exit(EXIT_FAILURE);
}

void tls_destroy(tls_context_t* context){
    if(munmap(context->unicity_gen.addr, sizeof(counter_t))==-1)exit(EXIT_FAILURE);

    sigset_t mask, oldmask;
    if(sigfillset(&mask)==-1) exit(EXIT_FAILURE);
    if(pthread_sigmask(SIG_BLOCK, &mask, &oldmask)!=0) exit(EXIT_FAILURE);
    if(close(context->unicity_gen.shm_fd)==-1) exit(EXIT_FAILURE);
    if(pthread_sigmask(SIG_SETMASK,&oldmask, NULL)!=0) exit(EXIT_FAILURE);

    if(mq_close(context->mqd)==-1) exit(EXIT_FAILURE);
}



typedef struct{
    sem_t sem;
    _Bool cancel;
    int state;
}return_t;

typedef struct{
    char nodename[256];
    char servname[33];
    struct addrinfo* addrinfo[2];
    int sockfd;
}open_t;

typedef struct{
    void* buf;
    size_t count;
}read_t;

typedef struct{
    void* buf;
    size_t count;
}write_t;

typedef struct{
}close_t;


typedef struct{
    unsigned char type;
    union{
        open_t o;
        read_t r;
        write_t w;
        close_t c;
    };
}processing_data_t;


typedef struct{
    return_t ret;
    processing_data_t data;
}oper_state_t;



typedef struct{
  int shm_fd;
  oper_state_t * addr;
} tls_oper_state_t;




static void oper_state_init(const tls_context_t* context, tls_oper_state_t* oper_state, uint64_t* oper_id_num, const unsigned char type){
    if(pthread_mutex_lock(&(context->unicity_gen.addr->mutex))!=0) exit(EXIT_FAILURE) ;
    (*oper_id_num)=((context->unicity_gen.addr->counter)++);
    if(pthread_mutex_unlock(&(context->unicity_gen.addr->mutex))!=0) exit(EXIT_FAILURE);


    char shm_name[78];
    strcpy(shm_name, PREFIX);
    char oper_id_str[16];
    if(sprintf(oper_id_str, "%"PRIX64, *oper_id_num )<0) exit(EXIT_FAILURE);
    strcat(shm_name,oper_id_str);
    strcat(shm_name,POSTFIX);

    if((oper_state->shm_fd=shm_open(shm_name,O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH))==-1)exit(EXIT_FAILURE);

    sigset_t mask, oldmask;
    if(sigfillset(&mask)==-1) exit(EXIT_FAILURE);
    if(pthread_sigmask(SIG_BLOCK, &mask, &oldmask)!=0) exit(EXIT_FAILURE);
    if(ftruncate(oper_state->shm_fd, sizeof(oper_state_t))==-1)exit(EXIT_FAILURE);
    if(pthread_sigmask(SIG_SETMASK,&oldmask, NULL)!=0) exit(EXIT_FAILURE);

    if((oper_state->addr=mmap(NULL, sizeof(oper_state_t), PROT_READ|PROT_WRITE, MAP_SHARED,oper_state->shm_fd,0))==MAP_FAILED)exit(EXIT_FAILURE);


    memset(oper_state->addr,0,sizeof(oper_state_t));
    if(sem_init(&(oper_state->addr->ret.sem),1,0u)==-1) exit(EXIT_FAILURE);
    oper_state->addr->ret.cancel=0;
    oper_state->addr->ret.state=0;
    oper_state->addr->data.type=type;
}




void tls_oper_state_destroy(tls_oper_state_t* oper_state){
 if(sem_destroy(&(oper_state->addr->ret.sem))==-1)exit(EXIT_FAILURE);
 if(munmap(oper_state->addr, sizeof(oper_state_t))==-1)exit(EXIT_FAILURE);

 sigset_t mask, oldmask;
 if(sigfillset(&mask)==-1) exit(EXIT_FAILURE);
 if(pthread_sigmask(SIG_BLOCK, &mask, &oldmask)!=0) exit(EXIT_FAILURE);
 if(close(oper_state->shm_fd)==-1) exit(EXIT_FAILURE);
 if(pthread_sigmask(SIG_SETMASK,&oldmask, NULL)!=0) exit(EXIT_FAILURE);
}




void tls_open(const tls_context_t* context, tls_oper_state_t* oper_state, const char* nodename, const char* servname){
    uint64_t oper_id_num;
    oper_state_init(context, oper_state, &oper_id_num, OPEN);
    strcpy(oper_state->addr->data.o.nodename, nodename);
    strcpy(oper_state->addr->data.o.servname, servname);
    oper_state->addr->data.o.addrinfo[0]==NULL;
    oper_state->addr->data.o.addrinfo[1]==NULL;
    oper_state->addr->data.o.sockfd=0;

    message_t m;
    memset(&m,0,sizeof(message_t));
    m.type=OPEN;
    m.oper_id=oper_id_num;
    m.connec_id=0;

    sigset_t mask, oldmask;
    if(sigfillset(&mask)==-1) exit(EXIT_FAILURE);
    if(pthread_sigmask(SIG_BLOCK, &mask, &oldmask)!=0) exit(EXIT_FAILURE);
    if(mq_send(context->mqd,(const char*)&m,sizeof(message_t),0)==-1)exit(EXIT_FAILURE);
    if(pthread_sigmask(SIG_SETMASK,&oldmask, NULL)!=0) exit(EXIT_FAILURE);
}



void tls_close(const tls_context_t* context, tls_oper_state_t* oper_state, const tls_connection_t* connection){
    uint64_t oper_id_num;
    oper_state_init(context, oper_state, &oper_id_num, CLOSE);

    message_t m;
    memset(&m,0,sizeof(message_t));
    m.type=CLOSE;
    m.oper_id=oper_id_num;
    m.connec_id=*connection;

    sigset_t mask, oldmask;
    if(sigfillset(&mask)==-1) exit(EXIT_FAILURE);
    if(pthread_sigmask(SIG_BLOCK, &mask, &oldmask)!=0) exit(EXIT_FAILURE);
    if(mq_send(context->mqd,(const char*)&m,sizeof(message_t),0)==-1)exit(EXIT_FAILURE);
    if(pthread_sigmask(SIG_SETMASK,&oldmask, NULL)!=0) exit(EXIT_FAILURE);

}




void tls_write(const tls_context_t* context, tls_oper_state_t* oper_state, const tls_connection_t* connection, const void* buf, const size_t count){
    uint64_t oper_id_num;
    oper_state_init(context, oper_state, &oper_id_num, WRITE);
    oper_state->addr->data.w.buf=buf;
    oper_state->addr->data.w.count=count;

    message_t m;
    memset(&m,0,sizeof(message_t));
    m.type=WRITE;
    m.oper_id=oper_id_num;
    m.connec_id=*connection;

    sigset_t mask, oldmask;
    if(sigfillset(&mask)==-1) exit(EXIT_FAILURE);
    if(pthread_sigmask(SIG_BLOCK, &mask, &oldmask)!=0) exit(EXIT_FAILURE);
    if(mq_send(context->mqd,(const char*)&m,sizeof(message_t),0)==-1)exit(EXIT_FAILURE);
    if(pthread_sigmask(SIG_SETMASK,&oldmask, NULL)!=0) exit(EXIT_FAILURE);


}



void tls_read(const tls_context_t* context, tls_oper_state_t* oper_state, const tls_connection_t* connection, const void* buf, size_t count){
    uint64_t oper_id_num;
    oper_state_init(context, oper_state, &oper_id_num, READ);
    oper_state->addr->data.r.buf=buf;
    oper_state->addr->data.r.count=count;

    message_t m;
    memset(&m,0,sizeof(message_t));
    m.type=READ;
    m.oper_id=oper_id_num;
    m.connec_id=*connection;

    sigset_t mask, oldmask;
    if(sigfillset(&mask)==-1) exit(EXIT_FAILURE);
    if(pthread_sigmask(SIG_BLOCK, &mask, &oldmask)!=0) exit(EXIT_FAILURE);
    if(mq_send(context->mqd,(const char*)&m,sizeof(message_t),0)==-1)exit(EXIT_FAILURE);
    if(pthread_sigmask(SIG_SETMASK,&oldmask, NULL)!=0) exit(EXIT_FAILURE);
}


_Bool tls_try(tls_oper_state_t* oper_state, int* ret){
    int ready;
    if(sem_getvalue(&(oper_state->addr->ret.sem), &ready)==-1)exit(EXIT_FAILURE);
    if(ready==0)return false;
    else {
        if(ret!=NULL) *ret=oper_state->addr->ret.state;
        return true;
    }
}


void tls_wait(tls_oper_state_t* oper_state, int* ret){
    int er_check;
    for(;;){
        if(sem_wait(&(oper_state->addr->ret.sem))==0)break;
        else{
            er_check=errno;
            if(er_check!=EINTR)exit(EXIT_FAILURE);
        }
    }
    if(ret!=NULL) *ret=oper_state->addr->ret.state;
}



// tls_try_cancel(const tls_context* context, oper_state* oper_state){

//}

//int tls_barier(const tls_context* context){}



//ready |^|
// /TLS_DAEMON_OPER_ID_ num + _UUIDDA85B873_13CB_417A_BF11_40B249701BA7
//    20 16 41 +1

//mq_send - три вещи : fd + oper_id+ oper_type
//на месте, создать
//не забыть про func free
//fd передаём через параметр connection

// sem_t - готово/неготово
// int ret - возвращаемое значение
// параметры + состояние выполнения + data

//начнём с tls_open

//sem_t state ret bool(cancel)
//params
//          open: (const char*, const char*) data - const char[256]nodename, const char[33]servname, struct addrinfo* addrinfo [2],
//                                           int sockfd; int check;
//          close: (void) //no data
//          read: (void* buf, size_t count) (без шифрования) data - void* buf, size_t count
//          write:(void* buf, size_t count) (без шифрования) data - void* buf, size_t count



//stop
// TODO: async-safety / mt-safety / signal-safety / EINTR handling
// убрать дублирование кода
















#endif
