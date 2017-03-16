/*
 * 370ct Assignment
 * Author: Rob Rigler
 * SID: 4939377
 * Tutor: Dr. Carey Pridgeon
 *
 */

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define NUM_WHEELS 6
#define LOG_BUFF_LENGTH 10
#define MIN_PROBLEMS_PER_SCENARIO 5

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t resume_condition = PTHREAD_COND_INITIALIZER;
pthread_cond_t wheelSetup_condition = PTHREAD_COND_INITIALIZER;
pthread_cond_t recoverySetup_condition = PTHREAD_COND_INITIALIZER;
pthread_cond_t updateVector_condition = PTHREAD_COND_INITIALIZER;
pthread_cond_t vectorUpdated_condition = PTHREAD_COND_INITIALIZER;
pthread_cond_t scenarioFinished_condition = PTHREAD_COND_INITIALIZER;
pthread_cond_t vectoring_condition = PTHREAD_COND_INITIALIZER;
pthread_cond_t problem_condition = PTHREAD_COND_INITIALIZER;
pthread_cond_t sinking_condition = PTHREAD_COND_INITIALIZER;
pthread_cond_t freeWheeling_condition = PTHREAD_COND_INITIALIZER;
pthread_cond_t blocked_condition = PTHREAD_COND_INITIALIZER;
pthread_cond_t working_condition = PTHREAD_COND_INITIALIZER;
pthread_barrier_t vector_barrier;

int pauseFlag = 0;
int stopLoggingFlag = 0;
int wheelSetupFlag = 0;
int recoverySetupFlag = 0;
int updateVectorFlag = 0;

double vectorDistance = 0;
int problemCount = 0;
typedef struct log_queue_t {
    pthread_mutex_t *lock;
    pthread_cond_t *buff_readable_cond;
    pthread_cond_t *buff_free_cond;
    char data[LOG_BUFF_LENGTH][20];
    char fileName[50];
    int head, tail;
} log_queue_t;

typedef enum wheel_state {
    WORKING,
    SINKING,
    FREEWHEELING,
    BLOCKED
} wheel_state;

typedef enum scenario_state {
    VECTORING,
    SOLVING
} scenario_state;
scenario_state s_state;

void *initScenario(void *args);
void *RoverController(void *args);
void *WheelController(void *args);
void *SolutionAndRecoveryController(void *args);
void *runWheel(void *args);
void *log_queue_add(log_queue_t * logQueue, char s[]);
void *log_queue_consume(void *args);
log_queue_t *log_queue_init(void * args);
void *log_queue_destroy(log_queue_t * logQueue);
void *checkScenarioCompletion();
void *checkForScenarioInterrupt();
void *runWheelState();
void *updateVector();
void *vectorEventReceiver();
wheel_state getRandomizedWheelState();
void *handleWheelState(wheel_state wS);

/*
 * main
 * Shows menu, handles input & starts scenarios*/
int main() {
    srand(time(NULL));
    char menuKeypress;
    int i = 0;
    pthread_t scenarioThread;
    pthread_barrier_init(&vector_barrier, NULL, NUM_WHEELS);

    // Main Loop.
    while (i == 0) {
        printf("\nSelect an option from the list below\n");
        printf("1. Scenario 1 - Single wheel Rock\n");
        printf("2. Scenario 2 - Single wheel sinking.\n");
        printf("3. Scenario 3 - Single wheel freewheeling.\n");
        printf("4. Scenario 4 - Multiple wheels encountering the same problem.\n");
        printf("Q. Exit\n");
        scanf(" %c", &menuKeypress);
        switch (menuKeypress) {
            case '1':
                break;
            case '2':
                break;
            case '3':
                break;
            case '4':
                break;
            case 'Q':
                i = 1;
                break;
            default:
                printf("%s%c%s","Unrecognised input ", menuKeypress, " try again. \n");
                continue;
        }
        pthread_create(&scenarioThread, NULL, initScenario, NULL);
        pthread_join(scenarioThread, NULL);
        printf("Finished Scenario.\n");

    }
    printf("\nPress any key to exit. \n");
    getchar();
    return 0;
}

/* intScenario
 *  - starts the chosen scenario
 * */
void *initScenario(void *args) {
    pthread_t rcc;
    stopLoggingFlag = 0;
    pthread_create(&rcc, NULL, RoverController, NULL);
    pthread_join(rcc, NULL);
    return 0;
}

void *RoverController(void * args) {
    pthread_t scT, wcT, lcT,veT;

    // Start Log Controller
    // Initialize scenario logger
    log_queue_t *logQueue = log_queue_init(NULL);
    pthread_create(&lcT, NULL, log_queue_consume, (void *)logQueue);
    log_queue_add(logQueue, "Started Logging");

    // Start vector event reciever
    //pthread_create(&veT, NULL, vectorEventReceiver, NULL);

    // Start Solution & Recovery Controller
    pthread_create(&scT, NULL, SolutionAndRecoveryController, (void *)logQueue);
    pthread_join(scT, NULL);

    // Start Wheel Controller
    pthread_create(&wcT, NULL, WheelController, (void *)logQueue);
    pthread_join(wcT, NULL);

    pthread_cond_wait(&scenarioFinished_condition, &mutex);
    printf("Scenario Finished");
    stopLoggingFlag = 1;
    return 0;
}

/*
 * SolutionAndRecoveryController
 *  - Starts threads responsible for dealing with wheel problems.*/
void *SolutionAndRecoveryController(void *args) {
    log_queue_t *logQueue = (log_queue_t *) args;
    log_queue_add(logQueue, "Started Solution & Recovery Controller");
    pthread_t sinkT, blockT, freeT;

    pthread_create(&sinkT, NULL, sinkEventReceiver, NULL);
    pthread_create(&sinkT, NULL, freeWheelEventReceiver, NULL);
    pthread_create(&sinkT, NULL, blockEventReceiver, NULL);


    return NULL;
}

void *sinkEventReceiver() {
    for (;;) {
        pthread_mutex_lock(&mutex);
        if (s_state != SINKING) {
            pthread_cond_wait(&sinking_condition, &mutex);
            printf("SinkHandler: I'M HELPING!");
        }
        pthread_mutex_unlock(&mutex);
    }
}

void *blockEventReceiver() {
    for (;;) {
        pthread_mutex_lock(&mutex);
        if (s_state != BLOCKED) {
            pthread_cond_wait(&blocked_condition, &mutex);
            printf("BlockHandler: I'M HELPING!");
        }
        pthread_mutex_unlock(&mutex);
    }
}

void *freeWheelEventReceiver() {
    for (;;) {
        pthread_mutex_lock(&mutex);
        if (s_state != FREEWHEELING) {
            pthread_cond_wait(&freeWheeling_condition, &mutex);
            printf("FreeHandler: I'M HELPING!");
        }
        pthread_mutex_unlock(&mutex);
    }
}

void *WheelController(void *args) {
    log_queue_t *logQueue = (log_queue_t *) args;
    pthread_t wheels[6];

    log_queue_add(logQueue, "Spinning up wheels...");

    for (int i =0; i < NUM_WHEELS; i++) {
        int *arg = malloc(sizeof(*arg));
        *arg = i;
        pthread_create(&wheels[i], NULL, runWheel, arg);
    }
    for(int i =0; i< NUM_WHEELS; i++) {
        pthread_join(wheels[i], NULL);
    }
    return NULL;
}

/*
 * runWheel
 *  - Simulates an individual wheel
 *  */
void *runWheel(void *args) {
    int a = *((int *) args);
    printf("%d: I'm a Wheel!\n", a);

    for (;;) {
        checkScenarioCompletion(); // Has the Rover solved 5 problems or vectored 2 units?
        checkForScenarioInterrupt(); // Check if the scenario is paused or terminated.
        runWheelState();
        pthread_cond_wait(&vectoring_condition, &mutex);
        updateVector(); // Update the position of the Rover, once all threads have reached barrier.
    }
    free(args);
    return 0;
}

void *checkScenarioCompletion() {
    pthread_mutex_lock(&mutex);
    if (vectorDistance >= 6.000000 || problemCount == MIN_PROBLEMS_PER_SCENARIO) {
        printf("MISCHEIVE MANAGED\n");
        pthread_cond_broadcast(&scenarioFinished_condition);
        pthread_exit(NULL);
    }
    pthread_mutex_unlock(&mutex);
    return NULL;
}

void *checkForScenarioInterrupt() {
    return NULL;
}


void *runWheelState() {
    pthread_mutex_lock(&mutex);
    if (s_state == PROBLEM_SOLVING) {
        pthread_cond_wait(&vectoring_condition, &mutex);
    }
    wheel_state newState = getRandomizedWheelState();
    handleWheelState(newState);
    pthread_mutex_unlock(&mutex);
    return NULL;
}

void *handleWheelState(wheel_state wS) {
    // We are already locked in the mutex, when inside this function
    if (wS == WORKING) {
        s_state = VECTORING;
        pthread_cond_broadcast(&vectoring_condition);
    }
    else if (wS == SINKING) {
        s_state = SINKING;
        pthread_cond_broadcast(&sinking_condition);
    }
    else if (wS == FREEWHEELING) {
        s_state = FREEWHEELING;
        pthread_cond_broadcast(&freeWheeling_condition);
    }
    else if (wS == BLOCKED) {
        s_state = BLOCKED;
        pthread_cond_broadcast(&blocked_condition);
    }
}

wheel_state getRandomizedWheelState() {
    int r = rand() % 100;
    if (r <= 40 ) {
        return WORKING;
    }
    if (r <= 60) {
        return SINKING;
    }
    if (r <= 80) {
        return FREEWHEELING;
    }
    if (r <= 100) {
        return BLOCKED;
    }
}

/* Updates total vectored distance after each wheel iteration.*/
void *updateVector() {
    pthread_barrier_wait(&vector_barrier);
    pthread_mutex_lock(&mutex);
    vectorDistance += 0.1;
    pthread_mutex_unlock(&mutex);
    return NULL;
}

// initialises the circular buffer & corresponding mutex for Logging.
log_queue_t *log_queue_init(void *args) {
    log_queue_t *logQueue = malloc(sizeof(log_queue_t));
    if (logQueue == NULL) {
        return NULL;
    };

    logQueue->lock = malloc(sizeof(pthread_mutex_t));
    if (logQueue->lock == NULL) {
        free(logQueue);
        return NULL;
    }
    pthread_mutex_init(logQueue->lock, NULL);

    logQueue->buff_readable_cond = malloc(sizeof(pthread_cond_t));
    if (logQueue->buff_readable_cond == NULL) {
        free(logQueue);
        return NULL;
    }
    pthread_cond_init(logQueue->buff_readable_cond, NULL);

    logQueue->buff_free_cond = malloc(sizeof(pthread_cond_t));
    if (logQueue->buff_free_cond == NULL) {
        free(logQueue);
        return NULL;
    }
    pthread_cond_init(logQueue->buff_free_cond, NULL);

    logQueue->head = 0;
    logQueue->tail = 0;
    strcpy(logQueue->fileName, "test.txt");
    return logQueue;
}

void *log_queue_add(log_queue_t * logQueue, char s[]) {
    pthread_mutex_lock(logQueue->lock);
    int next = logQueue->head + 1;
    if (next >= LOG_BUFF_LENGTH)
        next = 0;

    // buffer is full, wait for consumer to catch up
    if (next == logQueue->tail)
        pthread_cond_wait(logQueue->buff_free_cond, logQueue->lock);

    strcpy(logQueue->data[logQueue->head], s);
    pthread_cond_signal(logQueue->buff_readable_cond);
    logQueue->head = next;
    pthread_mutex_unlock(logQueue->lock);
    return 0;
}

void *log_queue_consume(void *args) {
    FILE *fp;
    log_queue_t *logQueue = (log_queue_t *) args;
    fp = fopen(logQueue->fileName, "w+");
    for (;;) {
        pthread_mutex_lock(logQueue->lock);
        if (stopLoggingFlag == 1) {
            printf("Cleaning up my mess");
            fclose(fp);
            pthread_mutex_unlock(logQueue->lock);
            log_queue_destroy(logQueue);
            return NULL;
        }
        // if the head isn't ahead of the tail, we don't have anything to consume.
        if (logQueue->head == logQueue->tail) {
            pthread_cond_wait(logQueue->buff_readable_cond, logQueue->lock);
        }

        // print to console
        printf("%s\n", logQueue->data[logQueue->tail]);

        // write to file
        // TODO: THIS DOES NOT WRITE TO FILE
        fprintf(fp, "%s\n", logQueue->data[logQueue->tail]);

        // TODO: This only clears first char from element.
        // clear from buffer
        strcpy(logQueue->data[logQueue->tail], "0");

        // singal buff_free_cond
        pthread_cond_signal(logQueue->buff_free_cond);

        int next = logQueue->tail + 1;
        if(next >= LOG_BUFF_LENGTH)
            next = 0;

        logQueue->tail = next;
        pthread_mutex_unlock(logQueue->lock);
    }
}

void *log_queue_destroy(log_queue_t *logQueue) {
    // Possibly request lock before destroying?
    pthread_mutex_destroy(logQueue->lock);
    free(logQueue);
    return 0;
}
