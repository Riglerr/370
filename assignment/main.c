#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

/*
 * 370 ASSIGNMENT
 * This program starts 4 threads,
 * threads are unlocked based on the value of a condition variable and a state*/
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t s1_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t s2_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t s3_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t s4_cond = PTHREAD_COND_INITIALIZER;

static int cond = 0;

void* generateNewRandomValue();
void *calculateNextState();
enum { s1, s2, s3, s4, p} state = s1;
int condVal = 1;

void* p1(void *arg) {
    for (;;) {
        pthread_mutex_lock(&mutex);
        if (state != s1) pthread_cond_wait(&s1_cond, &mutex);
        printf( "P1, Running");
        generateNewRandomValue(NULL);
        calculateNextState();
        pthread_mutex_unlock(&mutex);
    }
}

void* p2(void *arg) {
    for (;;) {
        pthread_mutex_lock(&mutex);
        if (state != s2) pthread_cond_wait(&s2_cond, &mutex);
        printf("P2, Running");
        generateNewRandomValue(NULL);
        calculateNextState();
        pthread_mutex_unlock(&mutex);
    }
}

void* p3(void *arg) {
    for (;;) {
        pthread_mutex_lock(&mutex);
        if (state != s3) pthread_cond_wait(&s3_cond, &mutex);
        printf("P3, Running");
        generateNewRandomValue(NULL);
        calculateNextState();
        pthread_mutex_unlock(&mutex);
    }
}

void* p4(void *arg) {
    for (;;) {
        pthread_mutex_lock(&mutex);
        if (state != s4) pthread_cond_wait(&s4_cond, &mutex);
        printf( "P4, Running");
        generateNewRandomValue(NULL);
        calculateNextState();
        pthread_mutex_unlock(&mutex);
    }
}

void* generateNewRandomValue() {
    condVal = (rand()%(1-4))+1;
    return NULL;
}

void *calculateNextState() {
    if (condVal == 4) {
        state = s4;
        pthread_cond_signal(&s4_cond);
    }
    else if (condVal == 3) {
        state = s3;
        pthread_cond_signal(&s3_cond);
    }
    else if (condVal == 2) {
        state = s2;
        pthread_cond_signal(&s2_cond);
    }
    else if (condVal == 1) {
        state = s1;
        pthread_cond_signal(&s1_cond);
    }
    return NULL;
}

void stop() {
    pthread_mutex_lock(&mutex);
    state = p;
    pthread_mutex_unlock(&mutex);
}

void* haltThread() {
    char keypress;
    for(;;) {
        keypress = getchar();
        printf("%c",keypress);
        printf("%d\n", state);
        switch(keypress) {
            case 'p':
                stop();
                break;
            default: break;
        }
    }
    return NULL;
}
int main() {
    pthread_t t1, t2, t3, t4, tH;
    void * exitStatus;
    int rc;

    srand(time(NULL));

    pthread_create(&tH, NULL, haltThread, NULL);
    pthread_create(&t1, NULL, p1, NULL);
    pthread_create(&t2, NULL, p2, NULL);
    pthread_create(&t3, NULL, p3, NULL);
    pthread_create(&t4, NULL, p4, NULL);
    calculateNextState();
    pthread_join(t1, &exitStatus);
    pthread_join(t2, &exitStatus);
    pthread_join(t3, &exitStatus);
    pthread_join(t4, &exitStatus);

    getchar();
    return 0;
}