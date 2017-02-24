#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int cond = 0;

enum { p1, p2, p3, p4} state = p1;
srand(time(NULL));
int p1(void *arg) {
    for (;;) {
        pthread_mutex_lock(&mutex);
    }
}

int p2(void *arg) {
    for (;;) {
        pthread_mutex_lock(&mutex);
    }
}

int p3(void *arg) {
    for (;;) {
        pthread_mutex_lock(&mutex);
    }
}

int p4(void *arg) {
    for (;;) {
        pthread_mutex_lock(&mutex);
    }
}

int assignRandomCond() {
    cond = (rand() % 4) + 1;
    return 0;
}

int main() {
    pthread_t t1, t2, t3, t4;
    void * exitStatus;
    int rc;
    pthread_create(&t1, NULL, p1, null);
    pthread_create(&t2, NULL, p2, null);
    pthread_create(&t3, NULL, p3, null);
    pthread_create(&t4, NULL, p4, null);

    pthread_join(t1, &exitStatus);
    pthread_join(t2, &exitStatus);
    pthread_join(t3, &exitStatus);
    pthread_join(t4, &exitStatus);

    getchar();
    return 0;
}