#include <stdio.h>
#include <pthread.h>

typedef enum {FULL, NOT_FULL} BuffState;

typedef struct AppState {
    pthread_mutex_t *mutex;
    pthread_cond_t *buffFullCond;
    pthread_cond_t *buffFreeCond;
    unsigned int buffLength;
    char buff[10][50];
    BuffState bufferState;
    unsigned int linesInFile;
    unsigned int writePos;
    unsigned int readPos;
} AppState;

void *printFile(pthread_mutex_t *mutex) {

}

void * incPos() {

}


int readLine(AppState* state, FILE* fp) {
    fgets(state->buff[state->linesInFile], sizeof(state->buff[state->linesInFile]), fp);
    state->linesInFile++;

    return 0;
}

void *readFile(void* arg) {
    FILE *fp;
    AppState *state = (AppState *)arg;
    fp = fopen("/home/rob/370/exercises/C3/caged_bird.txt", "r");

    if (feof(fp)) {
        pthread_exit(NULL);
    }
    if (state->writePos == 0 && state->readPos == 0) {
        readLine(state, fp);
        state->writePos++;
    }
    if (state->writePos == state->buffLength) {
        readLine(state, fp);
        state->writePos = 0;
    }
    else if (state->writePos < state->readPos) {
        readLine(state, fp);
    }
    else if (state->writePos > state->readPos) {

    }

    while(!feof(fp)) {
        // Request Lock
        pthread_mutex_lock(state->mutex);

        // Wait if buff is full
        if(state->bufferState == FULL)
            pthread_cond_wait(state->buffFreeCond, state->mutex);

        pthread_mutex_unlock(state->mutex);
    }
    fclose(fp);
    pthread_exit(NULL);
    return NULL;
}

void printBufferStatus(AppState *state) {
    int i = 0,j = 0, k = 0;
    printf("\n");
    for(i; i < state->buffLength; i++) {
        if (i == state->readPos) {
            printf("%s", "R ");
        }
        else if(i == state->writePos) {
            printf("%s", "W ");
        }
        else {
            printf("%s", "  ");
        }
    }
    printf("\n");
    for(j; j < state->buffLength; j++) {
        if (j == state->buffLength -1) {
            printf("%s", "|");
        }
        else {
            printf("%s", "|_");
        }
    }
    printf("\n");
    for(k; k < state->buffLength; k++) {
        printf("%d%s", k, " ");
    }
}

int main() {
    // Init Types
    pthread_t menuThread, fileThread, printThread, incThread;
    AppState state;
    AppState *ptr = &state;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    // Init Data
    state.mutex = &mutex;
    state.linesInFile = 0;
    state.buffLength = 10;
    state.bufferState = NOT_FULL;
    state.readPos = 0;
    state.writePos = 0;

    // Start Threads
    pthread_create(&fileThread, NULL, readFile, ptr);
    pthread_join(fileThread, NULL);
    return 0;
}