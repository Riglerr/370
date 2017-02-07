#include <stdio.h>
#include <pthread.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t writeCondition = PTHREAD_COND_INITIALIZER;
pthread_cond_t readCondition = PTHREAD_COND_INITIALIZER;

typedef enum {NO_SPACE, AVAIL_SPACE} BuffState;
typedef struct BufferMatrix {
    char data[10][50];
    int length;
    int width;
    BuffState status;
} BufferMatrix;

//void printBufferStatus(AppState *state) {
//    int i = 0,j = 0, k = 0;
//    printf("\n");
//    for(i; i < state->buffLength; i++) {
//        if (i == state->writePos && state->writePos == state->readPos) {
//            printf("%s\n", "R ");
//            printf("%s", "W ");
//        }
//        else if (i == state->readPos) {
//            printf("%s", "R ");
//        }
//        else if(i == state->writePos) {
//            printf("%s", "W ");
//        }
//        else {
//            printf("%s", "  ");
//        }
//    }
//    printf("\n");
//    for(j; j < state->buffLength; j++) {
//        if (j == state->buffLength -1) {
//            printf("%s", "|");
//        }
//        else {
//            printf("%s", "|_");
//        }
//    }
//    printf("\n");
//    for(k; k < state->buffLength; k++) {
//        printf("%d%s", k, " ");
//    }
//}

void *writeDataToBuffer(void *arg) {
    BufferMatrix *buffer;
    FILE *fp;
    buffer = (BufferMatrix*)arg;
    fp = fopen("caged_bird.txt", "r");

    while(!feof(fp)) {
        // Wait while buffer has not bean read;
        pthread_mutex_lock(&mutex);
        if (buffer->status == NO_SPACE);
            pthread_cond_wait(&writeCondition, &mutex);
        pthread_mutex_unlock(&mutex);

        // Overwrite buffer
        int i = 0; // Control var for buff
        while (fgets(buffer->data[i], sizeof(buffer->data[i]), fp) && i < buffer->length) {
            i++;
        }
        // Signal consumer;
        pthread_mutex_lock(&mutex);
        buffer->status = NO_SPACE;
        pthread_cond_signal(&readCondition);
        pthread_mutex_lock(&mutex);
    }
//    pthread_mutex_lock(&mutex);
//    buffer->status = W_FINISH;
//    pthread_mutex_lock(&mutex);
    pthread_exit(NULL);
    return NULL;
}

void *readDataFromBuffer(void* arg) {
    BufferMatrix *buffer = (BufferMatrix*)arg;

    while(buffer->status != W_FINISH)
}

int main() {
    pthread_t writeThread, readThread;
    void *exitStatus;

    pthread_create(&writeThread, NULL, writeDataToBuffer, NULL);
    pthread_create(&readThread, NULL, readDataFromBuffer, NULL);
    pthread_join(writeThread, &exitStatus);
    pthread_join(readThread, &exitStatus);
    return 0;
}
