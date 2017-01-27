#include <pthread.h>
#include <stdio.h>

int i = 0;
void *printPoem(void *poem) {
    char (*array)[50][255] = poem;
    for (int j = 0; j < i; j++) {
        printf("%s", (*array)[j]); 
    }
    pthread_exit(NULL);
}

int main() {
   FILE *fp;
   char poem[50][255];

   fp = fopen("caged_bird.txt", "r");
   while(fgets(poem[i], sizeof(poem[i]), fp)) {
       i++;
   }
   if (feof(fp)) {
       //(sizeof poem / sizeof *poem)
       pthread_t threads[2];
       int p1 = pthread_create(&threads[0], NULL, printPoem, (void*)poem);
       int p2 = pthread_create(&threads[0], NULL, printPoem, (void*)poem);
       printPoem(poem);
   }
   else {
       // Error
       printf("HELP! Some bad shit went down.");
   }
   fclose(fp);
   pthread_exit(NULL);
}
