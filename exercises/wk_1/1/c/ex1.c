#include <stdio.h>
#include <pthread.h>

void *printInt(void *x) {
    printf("%d\n", x);
    pthread_exit(NULL);
}

int main( ) {
    pthread_t threads[2];
    long t1 = 15;
    long t2 = 17;
    pthread_create(&threads[0], NULL, printInt, (void*)t1);
    pthread_create(&threads[1], NULL, printInt, (void*)t2);
    pthread_exit(NULL);
    return 0;   
}
