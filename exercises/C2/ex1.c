#include <pthread.h>
#include <stdio.h>
#include <time.h>

int i = 0;
int j = 0;
int pos = 0;
int cv = 0;
int canPrint = 1;

void *printFile(void *poem) {
    while(j <= i -1) {
        if (cv == 0) 
            while(cv);
        cv = 0;
        char (*lineArray)[50][255] = poem;
        if (canPrint == 1) {
            printf("%d" pos, (*lineArray)[pos]);
            canPrint = 0;
            j++;    
        }
        cv = 1;
    }
    return NULL;
}

void *incPos() {
    while (j < i) {
        if (cv == 0) 
            while(cv);
        if (canPrint == 0) {
            cv = 0;
            pos++;
            canPrint = 1;
            cv = 1;
        }
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
       int p2 = pthread_create(&threads[1], NULL, incPos, NULL);
       pthread_join(threads[0], NULL);
       pthread_join(threads[1], NULL);
   }
   else {
       // Error
       printf("HELP! Some bad shit went down.");
   }
   fclose(fp);
}
