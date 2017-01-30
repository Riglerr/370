#include <pthread.h>
#include <stdio.h>
#include <time.h>

int i = 0;
int pos = 0;

void *printFile(void *poem) {
    struct timespec tim;
    tim.tv_sec = 0;
    tim.tv_nsec = 500000000L;
    char (*lineArray)[50][255] = poem;
    for (pos = 0; pos < i; pos++) {
        printf("%d: %s", pos, (*lineArray)[pos]); 
        nanosleep(&tim, NULL);
    }
    return NULL;
}

void *fuckWithPos(void *poem) {
    struct timespec tim;
    tim.tv_sec = 0;
    tim.tv_nsec = 100000000L;
    char (*lineArray)[50][255] = poem;
    for (pos = 0; pos < i; pos++) {
        nanosleep(&tim, NULL);
    }
    return NULL;
}

int main() {
   FILE *fp;
   char file[50][255];

   fp = fopen("caged_bird.txt", "r");
   while(fgets(file[i], sizeof(file[i]), fp)) {
       i++;
   }
   if (feof(fp)) {
       pthread_t threads[2];
       int p1 = pthread_create(&threads[0], NULL, printFile, (void*)file);
       int p2 = pthread_create(&threads[1], NULL, fuckWithPos, (void*)file);
       pthread_join(threads[0], NULL);
       pthread_join(threads[1], NULL);
   }
   else {
       // Error
       printf("HELP! Some bad shit went down.");
   }
   fclose(fp);
}
