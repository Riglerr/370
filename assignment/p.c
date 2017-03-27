#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define NUM_WHEELS 6
#define LOG_BUFF_LENGTH 10
#define MIN_PROBLEMS_PER_SCENARIO 5
#define MIN_VECTOR_DISTANCE 1

typedef enum wheel_state {
    WORKING,
    SINKING,
    FREEWHEELING,
    BLOCKED
} wheel_state;

typedef enum scenario_state {
    VECTORING,
    PROBLEM,
    SETUP,
    TERMINATE,
    COMPLETE
} scenario_state;

typedef enum scenario_type {
    ROCK_1,
    SINK_1,
    FREE_1,
    MULTI,
    FFA
} scenario_type;

typedef struct log_queue_t {
    pthread_mutex_t lock;
    int size;
    char data[10][255];
    int head, tail;
    pthread_cond_t write_cond;
    pthread_cond_t read_cond;
} log_queue_t;

typedef struct problem_conditions_t {
    pthread_cond_t sinking_condition;
    pthread_cond_t blocked_condition;
    pthread_cond_t freeWheeling_condition;
} problem_conditions_t;

typedef struct wheel_t {
    int id;
    wheel_state state;
} wheel_t;

typedef struct scenario_t {
    wheel_t wheels[NUM_WHEELS];
    scenario_state state;
    scenario_type type;
    pthread_mutex_t mutex;
    pthread_cond_t vector_condition;
    pthread_cond_t problem_condition;
    pthread_cond_t scenarioComplete_condition;
    int currentCycleProblems;
    int solvedProblemCount;
    double totalDistanceVectored;
    problem_conditions_t conditions;
    pthread_barrier_t wheelSetup_barrier;
    pthread_barrier_t solutionSetup_barrier;
    pthread_barrier_t wheelCylce_barrier;
    log_queue_t *log;
} scenario_t;

typedef struct scenario_wheel_t {
    scenario_t *scenario;
    wheel_t *wheel;
} scenario_wheel_t;

int isScenarioComplete(scenario_t *scenario);
void *checkForScenarioInterrupt();
wheel_state getRandomizedWheelState();

int scenario_init(scenario_t *scenario);
int scenario_start(scenario_t *scenario);
void *wheel_start(void *args);

void *sinkProblemHandler(void * args);
void *freeWheelProblemHandler(void * args);
void *blockProblemHandler(void * args);
void *vectorMonitor(void * args);
wheel_state randomizeSingleState(wheel_state secondaryState);
wheel_state getNextWheelStateForScenario(scenario_t *scenario);

int log_init(log_queue_t *log);
void *log_consume(void *args);
void log_print(log_queue_t *log, char *string);

/*
 * main
 * Shows menu, handles input & starts scenarios*/
int main() {
    srand(time(NULL));
    scenario_t scenario;
    char menuKeypress;
    int i = 0;
    // Main Loop.
    while (i == 0) {
        printf("\nSelect an option from the list below\n");
        printf("1. Scenario 1 - Single wheel Rock\n");
        printf("2. Scenario 2 - Single wheel sinking.\n");
        printf("3. Scenario 3 - Single wheel freewheeling.\n");
        printf("4. Scenario 4 - Multiple wheels encountering the same problem.\n");
        printf("5. Scenario 5 - Free-For-All. (Any problem, any number of wheels)\n");
        printf("Q. Exit\n");
        scanf(" %c", &menuKeypress);
        switch (menuKeypress) {
            case '1':
                scenario.type = ROCK_1;
                break;
            case '2':
                scenario.type = SINK_1;
                break;
            case '3':
                scenario.type = FREE_1;
                break;
            case '4':
                scenario.type = MULTI;
                break;
            case '5':
                scenario.type = FFA;
                break;
            case 'Q':
                i = 1;
                continue;
            default:
                printf("Unrecognised input: %c \n", menuKeypress);
                continue;
        }
        scenario_init(&scenario);
        scenario_start(&scenario);
        log_print(scenario.log, "SCENARIO COMPLETE.\n");
        pthread_cond_broadcast(&scenario.log->read_cond);

    }
    printf("\nPress any key to exit. \n");
    getchar();
    return 0;
}

// Initialises/Resets the scenario object.
int scenario_init(scenario_t *scenario) {

    // Set counters & flags
    scenario->totalDistanceVectored = 0;
    scenario->solvedProblemCount = 0;
    scenario->state = SETUP;
    scenario->currentCycleProblems = 0;

    log_init(scenario->log);

    // Initialize wheels
    for (int i = 0; i < NUM_WHEELS; i++) {
        scenario->wheels[i].id = i;
        scenario->wheels[i].state = WORKING;
    }

    // Set mutexes & conditions.
    // Destroy & reinitialize if the already exist
    pthread_mutex_destroy(&scenario->mutex);
    pthread_mutex_init(&scenario->mutex, NULL);
    pthread_cond_init(&scenario->problem_condition, NULL);
    pthread_cond_init(&scenario->vector_condition, NULL);
    pthread_cond_init(&scenario->scenarioComplete_condition, NULL);
    pthread_barrier_init(&scenario->wheelSetup_barrier, NULL, NUM_WHEELS);
    pthread_barrier_init(&scenario->solutionSetup_barrier, NULL, 4);
    pthread_barrier_init(&scenario->wheelCylce_barrier, NULL, NUM_WHEELS + 1);

    pthread_cond_init(&scenario->conditions.sinking_condition, NULL);
    pthread_cond_init(&scenario->conditions.freeWheeling_condition, NULL);
    pthread_cond_init(&scenario->conditions.blocked_condition, NULL);
}

int scenario_start(scenario_t *scenario) {

    printf("Starting File Logger\n");
    pthread_t flT;
    pthread_create(&flT, NULL, log_consume, (void *)scenario);

    log_print(scenario->log, "Starting Solution Threads\n");
    pthread_t sinkT, blockT, freeT;
    pthread_create(&sinkT, NULL, sinkProblemHandler, (void *)scenario);
    pthread_create(&freeT, NULL, freeWheelProblemHandler, (void *)scenario);
    pthread_create(&blockT, NULL, blockProblemHandler, (void *)scenario);
    pthread_barrier_wait(&scenario->solutionSetup_barrier);

    pthread_mutex_lock(&scenario->mutex);
    scenario->state = VECTORING;
    pthread_mutex_unlock(&scenario->mutex);

    // Start VectorMonitor (Updates total distance travelled)
    log_print(scenario->log, "Starting VectorMonitor\n");
    pthread_t vMT;
    pthread_create(&vMT, NULL, vectorMonitor, (void *)scenario);

    // Start wheel threads
    pthread_t wheelThreads[NUM_WHEELS];
    scenario_wheel_t threadDataArr[NUM_WHEELS];
    for (int i =0; i < NUM_WHEELS; i++) {
        threadDataArr[i].scenario = scenario;
        threadDataArr[i].wheel = &scenario->wheels[i];
        pthread_create(&wheelThreads[i], NULL, wheel_start, (void *)&threadDataArr[i]);
    }

    // Wait for the Scenario to Finish.
    // This could also be achieved by doing a pthread_join() on each wheel thread?

    pthread_mutex_lock(&scenario->mutex);
    // while(scenario->state != TERMINATE) {
        pthread_cond_wait(&scenario->scenarioComplete_condition, &scenario->mutex);
        pthread_mutex_unlock(&scenario->mutex);
    // }
    return 0;
}

void *wheel_start(void *args) {
    scenario_wheel_t *threadData = (scenario_wheel_t *)args;
    scenario_t *scenario = threadData->scenario;
    wheel_t *wheel = threadData->wheel;
    log_queue_t *log = scenario->log;
    struct timespec ts;
    ts.tv_nsec = 0;
    ts.tv_sec = 1;
    char msg[255];
    while(scenario->state != TERMINATE) {
        // Check if the scenario is complete.. make sure mutex is locke beforehand
        pthread_mutex_lock(&scenario->mutex);
        if (isScenarioComplete(scenario) == 1) {
            scenario->state = COMPLETE;
            pthread_cond_broadcast(&scenario->scenarioComplete_condition);
            pthread_mutex_unlock(&scenario->mutex);
            pthread_exit(NULL);
        }

        // Pause if a problem is already being solved.
        // Get a new state for the wheel
        if (scenario->state != MULTI || (scenario->state == MULTI && scenario->currentCycleProblems == 0)) {
            wheel->state = getNextWheelStateForScenario(scenario);
        }

        while (scenario->state == PROBLEM) {
            sprintf(msg, "Wheel %d: Waiting for problems to be solved...\n", wheel->id);
            log_print(log, msg);
            pthread_cond_wait(&scenario->vector_condition, &scenario->mutex);
        }

        switch(wheel->state) {
            case WORKING:
                sprintf(msg, "Wheel %d: Vectoring...\n", wheel->id);
                log_print(log, msg);
                break;
            case SINKING:
                sprintf(msg, "Wheel %d: Sinking...\n", wheel->id);
                log_print(log, msg);
                scenario->currentCycleProblems +=1;
                scenario->state = PROBLEM;

                // Add wheel index to problem queue
                pthread_cond_signal(&scenario->conditions.sinking_condition);
                break;
            case BLOCKED:
                sprintf(msg, "Wheel %d: Blocked...\n", wheel->id);
                log_print(log, msg);
                scenario->currentCycleProblems += 1;
                scenario->state = PROBLEM;

                // Add wheel index to problem queue
                pthread_cond_signal(&scenario->conditions.blocked_condition);
                break;
            case FREEWHEELING:
                sprintf(msg, "Wheel %d: FreeWheeling...\n", wheel->id);
                log_print(log, msg);
                scenario->currentCycleProblems +=1;
                scenario->state = PROBLEM;

                // Add wheel index to problem queue
                pthread_cond_signal(&scenario->conditions.freeWheeling_condition);
                break;
        }
        if (wheel->state != WORKING) {
            while (wheel->state != WORKING && scenario->state != VECTORING) {
                pthread_cond_wait(&scenario->vector_condition, &scenario->mutex);
            }
        }
        pthread_mutex_unlock(&scenario->mutex);
        pthread_barrier_wait(&scenario->wheelCylce_barrier);
        nanosleep(&ts,NULL);
    }
    pthread_exit(NULL);
    return NULL;
}

void *sinkProblemHandler(void *args) {
    scenario_t *scenario = (scenario_t *) args;
    while (scenario->state != TERMINATE) {
        pthread_mutex_lock(&scenario->mutex);
        if (isScenarioComplete(scenario) == 1) {
            pthread_mutex_unlock(&scenario->mutex);
            pthread_exit(NULL);
        }
        if (scenario->state == SETUP) {
            pthread_mutex_unlock(&scenario->mutex);
            pthread_barrier_wait(&scenario->solutionSetup_barrier);
            pthread_mutex_lock(&scenario->mutex);
        }

        log_print(scenario->log, "SinkHandler: Waiting for signal...\n");
        while(scenario->state != PROBLEM) {
            pthread_cond_wait(&scenario->conditions.sinking_condition, &scenario->mutex);
        }

        log_print(scenario->log, "\"SinkHandler: Signal Received, searching for problem\n");
        int x = 0;
        wheel_t *p;
        p = &scenario->wheels[0];
        // Deal with the first Sink problem
        for (int i =0; i < NUM_WHEELS; i ++) {
            if (p->state == SINKING) {
                x++;
                char msg[255];
                sprintf(msg, "SinkHandler: Resolving problem for wheel %d\n", p->id);
                log_print(scenario->log, msg);

                // Solve the problem
                p->state = WORKING;
                x--;
                break;
            }
            p++;
        }
        if (x == 0)
            scenario->state = VECTORING;

        log_print(scenario->log, "Sinkhandler: signaling & releasing lock...\n");
        pthread_cond_broadcast(&scenario->vector_condition);
        pthread_mutex_unlock(&scenario->mutex);

    }
    pthread_exit(NULL);
}

void *blockProblemHandler(void *args) {
    scenario_t *scenario = (scenario_t *) args;
    while (scenario->state != TERMINATE) {
        pthread_mutex_lock(&scenario->mutex);
        if (isScenarioComplete(scenario) == 1) {
            pthread_mutex_unlock(&scenario->mutex);
            pthread_exit(NULL);
        }
        if (scenario->state == SETUP) {
            pthread_mutex_unlock(&scenario->mutex);
            pthread_barrier_wait(&scenario->solutionSetup_barrier);
            pthread_mutex_lock(&scenario->mutex);
        }

        log_print(scenario->log, "BlockHandler: waiting for signal...\n");
        while(scenario->state != PROBLEM) {
            pthread_cond_wait(&scenario->conditions.blocked_condition, &scenario->mutex);
        }
        log_print(scenario->log, "BlockHandler: Signal Received, searching for problem\n");
        int x = 0;
        wheel_t *p;
        p = &scenario->wheels[0];
        // Deal with the first Sink problem
        for (int i =0; i < NUM_WHEELS; i ++) {
            if (p->state == BLOCKED) {
                x++;
                char msg[255];
                sprintf(msg, "BlockHandler: Resolving problem for wheel %d\n", p->id);
                log_print(scenario->log, msg);
                // Solve the problem
                p->state = WORKING;
                x--;
                break;
            }
            p++;
        }
        if (x == 0)
            scenario->state = VECTORING;
        log_print(scenario->log, "BlockHandler: signaling & releasing lock...\n");
        pthread_cond_broadcast(&scenario->vector_condition);
        pthread_mutex_unlock(&scenario->mutex);

    }
    pthread_exit(NULL);
}

void *freeWheelProblemHandler(void * args) {
    scenario_t *scenario = (scenario_t *) args;
    while (scenario->state != TERMINATE) {
        pthread_mutex_lock(&scenario->mutex);
        if (isScenarioComplete(scenario) == 1) {
            pthread_mutex_unlock(&scenario->mutex);
            pthread_exit(NULL);
        }
        if (scenario->state == SETUP) {
            pthread_mutex_unlock(&scenario->mutex);
            pthread_barrier_wait(&scenario->solutionSetup_barrier);
            pthread_mutex_lock(&scenario->mutex);
        }

        log_print(scenario->log, "Freehandler: waiting for signal...\n");
        while(scenario->state != PROBLEM) {
            pthread_cond_wait(&scenario->conditions.freeWheeling_condition, &scenario->mutex);
        }

        log_print(scenario->log, "FreeHandler: Signal Received, searching for problem\n");
        int x = 0;
        wheel_t *p;
        p = &scenario->wheels[0];
        // Deal with the first problem
        for (int i =0; i < NUM_WHEELS; i ++) {
            if (p->state == FREEWHEELING) {
                x++;
                char msg[255];
                sprintf(msg, "FreeHandler: Resolving problem for wheel %d\n", p->id);
                log_print(scenario->log, msg);
                // Solve the problem
                p->state = WORKING;
                x--;
                break;
            }
            p++;
        }
        if (x == 0)
            scenario->state = VECTORING;

        log_print(scenario->log, "Freehandler: signaling & releasing lock...\n");
        pthread_cond_broadcast(&scenario->vector_condition);
        pthread_mutex_unlock(&scenario->mutex);

    }
    pthread_exit(NULL);
}

void *checkForScenarioInterrupt() {
    return NULL;
}

int isScenarioComplete(scenario_t *scenario) {
    if (scenario->solvedProblemCount == MIN_PROBLEMS_PER_SCENARIO
        || scenario->totalDistanceVectored >= MIN_VECTOR_DISTANCE || scenario->state == TERMINATE) {
        return 1;
    }
    return 0;
}

wheel_state getRandomizedWheelState() {
    int r = rand() % 100;
    if (r <= 70 ) {
        return WORKING;
    }
    if (r <= 80) {
        return SINKING;
    }
    if (r <= 90) {
        return FREEWHEELING;
    }
    if (r <= 100) {
        return BLOCKED;
    }
}

wheel_state getNextWheelStateForScenario(scenario_t *scenario) {
    int i1,i2;
    wheel_state tmpState;
    switch (scenario->type) {
        case ROCK_1:
            if (scenario->currentCycleProblems >= 1)
                return WORKING;
            else return randomizeSingleState(BLOCKED);
            break;
        case SINK_1:
            if (scenario->currentCycleProblems >= 1)
                return WORKING;
            else return randomizeSingleState(SINKING);
            break;
        case FREE_1:
            if (scenario->currentCycleProblems >= 1)
                return WORKING;
            else return randomizeSingleState(FREEWHEELING);
            break;
        case MULTI:
            // Two random wheels, same condition.
            // First wheel gets random state,
            // Second wheel gets the same state.
            i1 = rand() % NUM_WHEELS;
            i2 = i1;
            while (i2 == i1) {
                i2 = rand() % NUM_WHEELS;
            }
            while( tmpState != WORKING) {
                tmpState = getRandomizedWheelState();
            }
            scenario->wheels[i1].state = tmpState;
            scenario->wheels[i2].state = tmpState;
            break;
        case FFA:
            return getRandomizedWheelState();
            break;
    }
}

/*
 * @name: randomizeSingleState
 * @description: Has a 25% chance to return the wheel state specified in secondaryState,
 *  returns WORKING by default
 * @returns wheel_state */
wheel_state randomizeSingleState(wheel_state secondaryState) {
    int r = rand() % 100;
    if (r <= 75 )
        return WORKING;
    if (r <= 100)
        return secondaryState;
}

void *vectorMonitor(void * args) {
    scenario_t *scenario = (scenario_t *)args;
    char msg[255];
    while(scenario->state != TERMINATE) {
        pthread_barrier_wait(&scenario->wheelCylce_barrier);
        pthread_mutex_lock(&scenario->mutex);
        scenario->totalDistanceVectored += 0.1;
        scenario->currentCycleProblems = 0;
        sprintf(msg, "TOTAL DISTANCE VECTORED: %f\n", scenario->totalDistanceVectored);
        log_print(scenario->log, msg);
        log_print(scenario->log, "=============================\n");
        pthread_mutex_unlock(&scenario->mutex);
    }
    pthread_exit(NULL);
}

int log_init(log_queue_t *log) {
    pthread_mutex_init(&log->lock, NULL);
    pthread_cond_init(&log->read_cond, NULL);
    pthread_cond_init(&log->write_cond, NULL);
    memset(log->data, '\0', sizeof(log->data));
    log->tail = 0;
    log->head =0;
    log->size = 10;
}

// Prints string & writes to file.
void log_print(log_queue_t *log, char string[]) {
    printf("%s", string);
    pthread_mutex_lock(&log->lock);
    // Wait for available buffer space
    while ((log->head + 1 % log->size) == log->tail) {
        pthread_cond_wait(&log->write_cond, &log->lock);
    }

    strcpy(log->data[log->head], string);
    pthread_cond_signal(&log->read_cond);

    // Increase head position, wrap if needed.
    log->head = log->head == log->size - 1 ? 0 : log->head + 1;
    pthread_mutex_unlock(&log->lock);

}

void *log_consume(void *args) {
    scenario_t *scenario = (scenario_t *)args;
    log_queue_t *log = scenario->log;
    FILE *fp;
    fp = fopen("test.txt","a");
    for (;;) {
        pthread_mutex_lock(&log->lock);
        if (scenario->state == COMPLETE && log->tail == log->head) {
            fclose(fp);
            pthread_exit(NULL);
        }
        // Wait if buffer empty
        while (log->tail == log->head) {
            pthread_cond_wait(&log->read_cond, &log->lock);
            if (scenario->state == COMPLETE) {
                fclose(fp);
                pthread_exit(NULL);
            }
        }

        // Write next string to file.
        fprintf(fp, "%s", log->data[log->tail]);

        //  Reset value
        memset(log->data[log->tail], '\0', sizeof(log->data[log->tail]));

        // Increment tail position, wrap if needed
        log->tail = log->tail + 1 == log->size - 1 ? 0 : log->tail + 1;
        pthread_cond_broadcast(&log->write_cond);
        pthread_mutex_unlock(&log->lock);
    }
}
