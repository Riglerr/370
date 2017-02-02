#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char keypress;
int linesInFile = 0;
int pos = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t      PrinterCond  = PTHREAD_COND_INITIALIZER;
pthread_cond_t      IncrementerCond  = PTHREAD_COND_INITIALIZER;
enum { PRINT, INCREMENT, PAUSE } prevState, state = PRINT;

void pause() {
    pthread_mutex_lock(&mutex);
    prevState = state;
    state = PAUSE;
    pthread_mutex_unlock(&mutex);
}

void resume() {
    pthread_mutex_lock(&mutex);
    if (prevState == PRINT) {
        state = INCREMENT;
        pthread_cond_signal(&IncrementerCond);
    } 
    else {
        state = PRINT;
        pthread_cond_signal(&PrinterCond);
    }
    pthread_mutex_unlock(&mutex);
}

void *printPos(void *poem) {
    char (*lineArray)[50][50] = poem;
    for(;;) {
        pthread_mutex_lock(&mutex);
        while(state != PRINT)
            pthread_cond_wait(&PrinterCond, &mutex);
        
        printf("%d: %s", pos, (*lineArray)[pos]);
        state = INCREMENT;
        pthread_cond_signal(&IncrementerCond);
        pthread_mutex_unlock(&mutex);
    }
}

void *incPos()  {
    for(;;) {
        // _lock requests ownership
        // It will block the current thread until it can unlock.
        pthread_mutex_lock(&mutex);
        while(state != INCREMENT)
            pthread_cond_wait(&IncrementerCond, &mutex);
        if(pos == linesInFile - 1) {
            pos = 0;
        }
        else {
            pos++;
        }
        state = PRINT;
        pthread_cond_signal(&PrinterCond);
        pthread_mutex_unlock(&mutex);
    }
}

void *showMenu() {
    for(;;) {
        keypress = getchar();
        printf("%c",keypress);
        printf("%d\n", state);
        switch(keypress) {
            // Quit
            case 'q': 
                exit(0);
                break;

            // Pause
            case 'p':
                pause();
                break;

            // Resume
            case 'r':
                resume();
                break;

            default: break;
        }
        printf("%d\n", state);
    }
}


int main() {
    // Declare Threads & exitStatus
    pthread_t menuThread, printThread, incThread;
    void *exitStatus;
    char file[50][50];
    int retCode;
    FILE *fp;

    fp = fopen("caged_bird.txt", "r");
    while(fgets(file[linesInFile], sizeof(file[linesInFile]), fp)) {
        linesInFile++;
    }
    if (!feof(fp)) {
        // Error
       printf("Some bad shit went down, Exiting.");
       fclose(fp);
       return 1;
    }
    fclose(fp);

    // printf("%s", (*lineArray)[0]);
    // printf("%s", (*lineArray)[1]);
    // printf("%s", (*lineArray)[2]);

    // Spin up menu, printer, & incrementer threads.
    pthread_create(&menuThread, NULL, showMenu, NULL);
    retCode = pthread_create(&printThread, NULL, printPos, file);
    retCode = pthread_create(&incThread, NULL, incPos, NULL);
    pthread_cond_signal(&PrinterCond);

    pthread_join(printThread, &exitStatus);
    pthread_join(incThread, &exitStatus);
    printf("Finished\n");
    getchar();
    return 0;
}
