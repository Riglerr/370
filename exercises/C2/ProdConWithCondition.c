#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <sched.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <string.h>
char consume_char (FILE * in) {
    char a;
    a = fgetc (in);
    return a;
}
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t      ProducerCond  = PTHREAD_COND_INITIALIZER;
pthread_cond_t      ConsumerCond  = PTHREAD_COND_INITIALIZER;
enum { STATE_A, STATE_B } state = STATE_A;
// struct to hold the data to tranfer
typedef struct input {
    char data[1024];
    int len;
    int dataIndex;
    int chunks;
}
input;
typedef struct received {
    char data[1024];
    int len;
    int chunks;
    int totalIncoming;
}
received;
typedef struct buffStruct {
    char buff[10];
    //where we are in the buffer
    int buffIndex;
    int status;
    int buffSize;
}
buffStruct;
buffStruct buffer;
char keypress;
// functions for use in threads should return a void pointer and any arguments should be passed as void
// references
// the function needs to cast the void parameter to the required type before
// it can make use of it.
// note, threads can only take a single parameter, so if more is needed, a compound type must be used.

void * producer ( void * arg ) {
    // this variable stores the return of the mutex locking and unlocking procedure
    // it can be left off, but we are using it here.
    int lock;
    int i;
    // cast the void parameter to required type
    input  incoming = *((input*)(arg));
    // index for source data
    incoming.dataIndex = 0;
    int j;
    for (;;) {
        /* Wait for state A */
        pthread_mutex_lock(&mutex);
        while (state != STATE_A)
            pthread_cond_wait(&ProducerCond, &mutex);
        pthread_mutex_unlock(&mutex);
        //usleep(200);
        j = buffer.buffIndex-1;
        if (incoming.dataIndex<incoming.len) {
            for (i=j;i>-1;i--) {
                buffer.buff[i] = incoming.data[incoming.dataIndex];
                printf("> Producer added '%c' to buffer index %i\n",incoming.data[incoming.dataIndex],i);
                incoming.dataIndex++;
            }
            printf("\n");
        }
        // hand control to consumer
        pthread_mutex_lock(&mutex);
        state = STATE_B;
        pthread_cond_signal(&ConsumerCond);
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}
void * consumer ( void * arg ) {
    // this variable stores the return of the mutex locking and unlocking procedure
    // it can be left off.
    int lock;
    int i;
    int k;
    int l;
    // count for how much data have been transferred
    int transferred = 0;
    // cast the void parameter to received type
    received  incoming = *((received*)(arg));
    for (;;) {
        pthread_mutex_lock(&mutex);
        while (state != STATE_B)
            pthread_cond_wait(&ConsumerCond, &mutex);
        pthread_mutex_unlock(&mutex);
        //usleep(200);
        if (transferred >= incoming.totalIncoming) {
            printf("> Data transferred\n");
            printf("\n> Destination array contains: '");
            for (l=0;l<incoming.totalIncoming;l++) {
                printf("%c",incoming.data[l]);
            }
            printf("'\n\n");
            printf("> Press enter key to exit program\n");
            getchar();
            exit(0);
        }
        // print out the buffer element/s we are removing
        for (k=buffer.buffSize-1;k>((buffer.buffSize-1)-incoming.chunks);k--) {
            if(incoming.len<incoming.totalIncoming) {
                printf("> Consumer removed: '%c' from buffer index %i\n",buffer.buff[k],k);
                incoming.data[incoming.len] = buffer.buff[k];
                incoming.len++;
            }
        }
        printf("\n> Destination array contains: '");
        for (l=0;l<incoming.len;l++) {
            printf("%c",incoming.data[l]);
        }
        printf("'\n\n");
        // shift buffer content forward, freeing the same amount of elements as we have removed
        int x= (buffer.buffSize-1)-incoming.chunks;
        printf("> Beginning Buffer Shift Operation\n");
        for (i=x;i>-1;i--) {
            printf("> Shifting element '%c' from buffer index %i to buffer index %i\n",buffer.buff[i+incoming.chunks],i,i+incoming.chunks);
                buffer.buff[i+incoming.chunks] = buffer.buff[i];
        }
        int y;
        printf("> Completed Buffer Shift Operation\n");
        printf("\n> Flushing ready portion of buffer\n");
        for (i=0;i<incoming.chunks;i++) {
            buffer.buff[i] = '#';
        }
        printf("\n> Buffer now contains:\n");
        for (i=buffer.buffSize-1;i>-1;i--) {
            printf("| %i ",i);
        }
        printf("|\n");
        for (i=buffer.buffSize-1;i>-1;i--) {
            printf("| %c ",buffer.buff[i]);
        }
        printf("|\n");
        // we have made some space in the buffer, change the buffer index to reflect this
        buffer.buffIndex=incoming.chunks;
        // and update the number of characters transferred
        transferred+=incoming.chunks;
        //}
        printf("\n> Transferred %i of %i characters from Producer to Consumer\n\n",incoming.len,incoming.totalIncoming);
        
        pthread_mutex_lock(&mutex);
        state = STATE_A;
        pthread_cond_signal(&ProducerCond);
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}
// simple menu thread that allows us to exit the program
void *menu(void *param) {
    for (;;) {
        keypress=getchar();
        printf("%c",keypress);
        if(keypress=='q') {
            exit(0);
        }
    }
}
// main function
int main ( void) {
    pthread_t menuThread,thread_ID1, thread_ID2;
    void * exitstatus;
    buffer.buffIndex = 10;
    buffer.buffSize = 10;
    int chunksSize = 3;
    buffer.status = 0;
    //  start in write state for buffer
    printf("370CT pthreads producer consumer example 1 \n");
    int        rc;
    input source;
    // fill the source data array with null characters
    memset(source.data,'\0',1024);
    strcpy(source.data,"Mary Had A Little Lamb, It's Fleece Was White As Snow");
    source.len = 53;
    //set the number of buffer elements to remove each time
    source.chunks = chunksSize;
    received destination;
    memset(destination.data,'\0',1024);
    //set the number of buffer elements to remove each time
    destination.chunks = chunksSize;
    // set destination data length to 0
    destination.len = 0;
    // set how much data the consumer should expect
    destination.totalIncoming = source.len;
    pthread_create (&menuThread , NULL, menu, NULL) ;
    rc = pthread_create (&thread_ID1 , NULL, producer, &source);
    printf("> Return code from pthread_create is %d \n", rc);
    if(rc) {
        printf("\n Return code from pthread_create is %d. Thread not created \n", rc);
        exit(1);
    }
    rc = pthread_create (&thread_ID2 , NULL, consumer, &destination);
    printf("> Return code from pthread_create is %d \n", rc);
    if(rc) {
        printf("\n Return code from pthread_create is %d. Thread not created \n", rc);
        exit(1);
    }
    pthread_join ( thread_ID1 , &exitstatus );
    pthread_join ( thread_ID2 , &exitstatus );
    printf("Finished\n");
    getchar();
    return 0;
}
