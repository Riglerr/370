#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define NUM_WHEELS 6
#define MIN_PROBLEMS_PER_SCENARIO 5
#define MIN_VECTOR_DISTANCE 2

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
    char *fileName;
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
    problem_conditions_t conditions;
    pthread_barrier_t wheelSetup_barrier;
    pthread_barrier_t solutionSetup_barrier;
    pthread_barrier_t wheelCycle_barrier;
    log_queue_t log;
    int currentCycleProblems;
    int solvedProblemCount;
    double totalDistanceVectored;
    int multiReset;
} scenario_t;

typedef struct scenario_wheel_t {
    scenario_t *scenario;
    wheel_t *wheel;
} scenario_wheel_t;

int isScenarioComplete(scenario_t *scenario);
wheel_state getRandomizedWheelState();

void *scenario_create(void *args);
void scenario_destroy(scenario_t *scenario);
scenario_t scenario_init(scenario_type scType);
int scenario_run(scenario_t *scenario);

void *menuLoop();
void *wheel_start(void *args);

void *sinkProblemHandler(void * args);
void *freeWheelProblemHandler(void * args);
void *blockProblemHandler(void * args);
void *vectorMonitor(void * args);
wheel_state randomizeSingleState(wheel_state secondaryState);
wheel_state randomizeStateForScenario(scenario_t *scenario);

log_queue_t log_init(scenario_type scType);
void log_destroy(log_queue_t *log);
void *log_consume(void *args);
void log_print(log_queue_t *log, char *string);
char* generateFileName(scenario_type scType);

/*
 * main
 * Shows menu, handles input & starts scenarios*/
int main() {
    srand(time(NULL));
    pthread_t menuThread;

    // Kick off menu thread
    pthread_create(&menuThread, NULL, menuLoop, NULL);
    pthread_join(menuThread, NULL);

    printf("\nPress any key to exit. \n");
    getchar();
    return 0;
}

void *menuLoop() {
    scenario_type type;
    pthread_t scenarioThread;
    int scenarioRetVal;
    void *scenarioStatus;
    char menuKeypress;
    int exitFlag = 0;

    while (exitFlag == 0) {
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
                type = ROCK_1;
                break;
            case '2':
                type = SINK_1;
                break;
            case '3':
                type = FREE_1;
                break;
            case '4':
                type = MULTI;
                break;
            case '5':
                type = FFA;
                break;
            case 'Q':
                exitFlag = 1;
                continue;
            default:
                printf("Unrecognised input: %c \n", menuKeypress);
                continue;
        }

        // Start the Scenario
        scenarioRetVal = pthread_create(&scenarioThread, NULL, scenario_create, (void *)type);
        if (scenarioRetVal) {
            printf("ERROR(scenario_create); return code from pthread_create() is %d\n", scenarioRetVal);
        }
        else {
            scenarioRetVal = pthread_join(scenarioThread, &scenarioStatus);
            if (scenarioRetVal) {
                printf("ERROR; function scenario_create returned non-success code: %d\n", scenarioRetVal);
                break;
            }
        }

    }
    pthread_exit(0);
}


void *scenario_create(void *args) {
    scenario_type scType = (scenario_type)args;

    // Initialize Scenario Data
    scenario_t scenario = scenario_init(scType);

    // Run the scenario.
    scenario_run(&scenario);

    // Clean up my mess
    scenario_destroy(&scenario);
    pthread_exit(0);
}

scenario_t scenario_init(scenario_type scType) {
    scenario_t scenario;

    scenario.type = scType;

    // Set counters & flags
    scenario.totalDistanceVectored = 0;
    scenario.solvedProblemCount = 0;
    scenario.currentCycleProblems = 0;
    scenario.multiReset = 0;

    // Init scenario state
    scenario.state = SETUP;

    // Setup Logging utility
    scenario.log = log_init(scType);

    // Initialize wheel id & states
    for (int i = 0; i < NUM_WHEELS; i++) {
        scenario.wheels[i].id = i;
        scenario.wheels[i].state = WORKING;
    }

    // Init pthread vars
    pthread_mutex_init(&scenario.mutex, NULL);
    pthread_cond_init(&scenario.problem_condition, NULL);
    pthread_cond_init(&scenario.vector_condition, NULL);
    pthread_cond_init(&scenario.scenarioComplete_condition, NULL);
    pthread_barrier_init(&scenario.wheelSetup_barrier, NULL, NUM_WHEELS);
    pthread_barrier_init(&scenario.solutionSetup_barrier, NULL, 5);
    pthread_barrier_init(&scenario.wheelCycle_barrier, NULL, NUM_WHEELS + 1);
    pthread_cond_init(&scenario.conditions.sinking_condition, NULL);
    pthread_cond_init(&scenario.conditions.freeWheeling_condition, NULL);
    pthread_cond_init(&scenario.conditions.blocked_condition, NULL);

    return scenario;
}

int scenario_run(scenario_t *scenario) {

    pthread_t fLoggerThread; // File Logger Thread
    pthread_t sinkT, blockT, freeT; // Problem handler Threads
    pthread_t vMT; // Vector Monitor Thread

    printf("Starting File Logger\n");
    pthread_create(&fLoggerThread, NULL, log_consume, (void *)scenario);

    printf("Logging to: %s\n", scenario->log.fileName);
    log_print(&scenario->log, "Starting Solution Threads\n");
    pthread_create(&sinkT, NULL, sinkProblemHandler, (void *)scenario);
    pthread_create(&freeT, NULL, freeWheelProblemHandler, (void *)scenario);
    pthread_create(&blockT, NULL, blockProblemHandler, (void *)scenario);
    pthread_barrier_wait(&scenario->solutionSetup_barrier);

    pthread_mutex_lock(&scenario->mutex);
    scenario->state = VECTORING;
    pthread_mutex_unlock(&scenario->mutex);

    // Start VectorMonitor (Updates total distance travelled)
    log_print(&scenario->log, "Starting VectorMonitor\n");
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
    while(scenario->state != TERMINATE && scenario->state != COMPLETE) {
        pthread_cond_wait(&scenario->scenarioComplete_condition, &scenario->mutex);
        pthread_mutex_unlock(&scenario->mutex);
    }

    for (int i = 0; i < NUM_WHEELS; i++) {
        pthread_join(wheelThreads[i], NULL);
    }
    // Ensure all threads are destroyed before exiting this scenario.
    // Signal the problem handler threads once more
    // This allows them to exit gracefully.
    //printf("Destroying Problem Handlers..\n");
    pthread_cond_signal(&scenario->conditions.blocked_condition);
    pthread_cond_signal(&scenario->conditions.sinking_condition);
    pthread_cond_signal(&scenario->conditions.freeWheeling_condition);
    pthread_join(sinkT, NULL);
    pthread_join(blockT, NULL);
    pthread_join(freeT, NULL);
    //printf("Problem Handlers successfully Destroyed\n");

    //printf("Waiting For File Logger to finish ...\n");
    pthread_join(fLoggerThread, NULL);
    //printf("File Logger sucessfully closed\n");

    //printf("Waiting for VectorMonitor ro finish...\n");
    pthread_join(vMT, NULL);
    //printf("VectorMonitor successfully closed.\n");

    return 0;
}

void scenario_destroy(scenario_t *scenario) {

    // Free pthread structs
    pthread_mutex_destroy(&scenario->mutex);
    pthread_cond_destroy(&scenario->problem_condition);
    pthread_cond_destroy(&scenario->vector_condition);
    pthread_cond_destroy(&scenario->scenarioComplete_condition);
    pthread_cond_destroy(&scenario->conditions.sinking_condition);
    pthread_cond_destroy(&scenario->conditions.freeWheeling_condition);
    pthread_cond_destroy(&scenario->conditions.blocked_condition);
    pthread_barrier_destroy(&scenario->wheelSetup_barrier);
    pthread_barrier_destroy(&scenario->solutionSetup_barrier);
    pthread_barrier_destroy(&scenario->wheelCycle_barrier);

    // Free Log
    log_destroy(&scenario->log);

    return;
}

void *wheel_start(void *args) {
    scenario_wheel_t *threadData = (scenario_wheel_t *)args;
    scenario_t *scenario = threadData->scenario;
    wheel_t *wheel = threadData->wheel;
    log_queue_t *log = &scenario->log;
    struct timespec ts;
    ts.tv_nsec = 0;
    ts.tv_sec = 1;
    char msg[255];
    pthread_barrier_wait(&scenario->wheelSetup_barrier);
    while(scenario->state != TERMINATE) {
        // Check if the scenario is complete.. make sure mutex is locke beforehand
        pthread_mutex_lock(&scenario->mutex);
        if (isScenarioComplete(scenario) == 1) {
            pthread_mutex_unlock(&scenario->mutex);
            pthread_exit(0);
        }

        if (scenario->type != MULTI)
            wheel->state = randomizeStateForScenario(scenario);

        // Block while another problem is being solved.
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
        pthread_barrier_wait(&scenario->wheelCycle_barrier);
        nanosleep(&ts,NULL); // Sleep for 1 Sec before continuing.
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

        log_print(&scenario->log, "SinkHandler: Waiting for signal...\n");
        while(scenario->state != PROBLEM) {
            pthread_cond_wait(&scenario->conditions.sinking_condition, &scenario->mutex);
            if (scenario->state == COMPLETE) {
                pthread_mutex_unlock(&scenario->mutex);
                pthread_exit(NULL);
            }
        }

        log_print(&scenario->log, "SinkHandler: Signal Received, searching for problem\n");
        int problems = 0;
        int attempts = 0;
        wheel_t *p;
        p = &scenario->wheels[0];
        // Deal with the first Sink problem
        for (int i =0; i < NUM_WHEELS; i ++) {
            if (p->state == SINKING) {
                attempts = 0;
                problems++;
                char msg[255];
                sprintf(msg, "SinkHandler: Resolving problem for wheel %d\n", p->id);
                log_print(&scenario->log, msg);

                // Solve the problem
                // Attempt to solve problem 3 times
                while ((p->state != WORKING && attempts < 4)) {
                    int outcome = rand() % 100;
                    if (outcome < 60) {
                        p->state = WORKING;
                    }
                    attempts++;
                }
                if (p->state != WORKING) {
                    log_print(&scenario->log, "Failed to solve problem after 3 attempts\n");
                    log_print(&scenario->log, "Terminating Scenario...\n");
                }
                problems--;
                break;
            }
            p++;
        }
        if (x == 0)
            scenario->state = VECTORING;

        log_print(&scenario->log, "Sinkhandler: signaling & releasing lock...\n");
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

        log_print(&scenario->log, "BlockHandler: waiting for signal...\n");
        while(scenario->state != PROBLEM) {
            pthread_cond_wait(&scenario->conditions.blocked_condition, &scenario->mutex);
            if (scenario->state == COMPLETE) {
                pthread_mutex_unlock(&scenario->mutex);
                pthread_exit(NULL);
            }
        }
        log_print(&scenario->log, "BlockHandler: Signal Received, searching for problem\n");
        int x = 0;
        wheel_t *p;
        p = &scenario->wheels[0];
        // Deal with the first Sink problem
        for (int i =0; i < NUM_WHEELS; i ++) {
            if (p->state == BLOCKED) {
                x++;
                char msg[255];
                sprintf(msg, "BlockHandler: Resolving problem for wheel %d\n", p->id);
                log_print(&scenario->log, msg);
                // Solve the problem
                p->state = WORKING;
                x--;
                break;
            }
            p++;
        }
        if (x == 0)
            scenario->state = VECTORING;
        log_print(&scenario->log, "BlockHandler: signaling & releasing lock...\n");
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

        log_print(&scenario->log, "Freehandler: waiting for signal...\n");
        while(scenario->state != PROBLEM && scenario->state != COMPLETE && scenario->state != TERMINATE) {
            pthread_cond_wait(&scenario->conditions.freeWheeling_condition, &scenario->mutex);
            if (scenario->state == COMPLETE) {
                pthread_mutex_unlock(&scenario->mutex);
                pthread_exit(NULL);
            }
        }

        if (scenario->state == COMPLETE && scenario->state == TERMINATE) {
            continue;
        }
        log_print(&scenario->log, "FreeHandler: Signal Received, searching for problem\n");
        int x = 0;
        wheel_t *p;
        p = &scenario->wheels[0];
        // Deal with the first problem
        for (int i =0; i < NUM_WHEELS; i ++) {
            if (p->state == FREEWHEELING) {
                x++;
                char msg[255];
                sprintf(msg, "FreeHandler: Resolving problem for wheel %d\n", p->id);
                log_print(&scenario->log, msg);
                // Solve the problem
                p->state = WORKING;
                x--;
                break;
            }
            p++;
        }
        if (x == 0)
            scenario->state = VECTORING;

        log_print(&scenario->log, "Freehandler: signaling & releasing lock...\n");
        pthread_cond_broadcast(&scenario->vector_condition);
        pthread_mutex_unlock(&scenario->mutex);

    }
    pthread_exit(NULL);
}

int isScenarioComplete(scenario_t *scenario) {
    if (scenario->solvedProblemCount == MIN_PROBLEMS_PER_SCENARIO
        || scenario->totalDistanceVectored >= MIN_VECTOR_DISTANCE || scenario->state == TERMINATE || scenario->state == COMPLETE) {
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

wheel_state randomizeStateForScenario(scenario_t *scenario) {
    int i1,i2;
    wheel_state tmpState = WORKING;
    switch (scenario->type) {
        case ROCK_1:
            if (scenario->currentCycleProblems >= 1)
                return WORKING;
            else return randomizeSingleState(BLOCKED);
        case SINK_1:
            if (scenario->currentCycleProblems >= 1)
                return WORKING;
            else return randomizeSingleState(SINKING);
        case FREE_1:
            if (scenario->currentCycleProblems >= 1)
                return WORKING;
            else return randomizeSingleState(FREEWHEELING);
        case MULTI:
            // Two random wheels, same condition.
            // First wheel gets random state,
            // Second wheel gets the same state.
            if (scenario->multiReset == 0) {
                tmpState = getRandomizedWheelState();
                if (tmpState != WORKING) {
                    i1 = rand() % NUM_WHEELS;
                    i2 = i1;
                    while (i2 == i1) {
                        i2 = rand() % NUM_WHEELS;
                    }
                    scenario->wheels[i1].state = tmpState;
                    scenario->wheels[i2].state = tmpState;
                    scenario->multiReset = 1;
                }
            }
            break;
        case FFA:
            return getRandomizedWheelState();
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
    while(scenario->state != TERMINATE && scenario->state != COMPLETE) {
        pthread_barrier_wait(&scenario->wheelCycle_barrier);
        pthread_mutex_lock(&scenario->mutex);
        scenario->totalDistanceVectored += 0.1;
        scenario->currentCycleProblems = 0;
        scenario->multiReset = 0;
        sprintf(msg, "TOTAL DISTANCE VECTORED: %f\n", scenario->totalDistanceVectored);
        log_print(&scenario->log, msg);
        log_print(&scenario->log, "=============================\n");
        if (isScenarioComplete(scenario) == 1) {
            log_print(&scenario->log, "SCENARIO COMPLETE\n");
            scenario->state = COMPLETE;
            pthread_cond_signal(&scenario->scenarioComplete_condition);
        }
        pthread_mutex_unlock(&scenario->mutex);
    }
    pthread_exit(NULL);
}

log_queue_t log_init(scenario_type scType) {
    log_queue_t log;

    // Init pthread vars
    pthread_mutex_init(&log.lock, NULL);
    pthread_cond_init(&log.read_cond, NULL);
    pthread_cond_init(&log.write_cond, NULL);

    // Erase buffer memory
    memset(log.data, '\0', sizeof(log.data));

    // Gen FileName for scenario
    log.fileName = generateFileName(scType);

    log.tail = 0;
    log.head =0;
    log.size = 10;

    return log;
}

// Prints string, adds it to the log_queue
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
    log_queue_t *log = &scenario->log;
    FILE *fp;
    fp = fopen(scenario->log.fileName,"a");
    if (fp == NULL) {
        perror( "Error opening file" );
        printf( "Error code opening file: %d\n", errno );
        printf( "Error opening file: %s\n", strerror( errno ) );
        exit(-1);
    }
    for (;;) {
        pthread_mutex_lock(&log->lock);
        if (scenario->state == COMPLETE && log->tail == log->head) {
            fclose(fp);
            pthread_exit(NULL);
        }
        // Wait if buffer empty
        while (log->tail == log->head) {
            if (scenario->state == SETUP) {
                pthread_mutex_unlock(&log->lock);
                pthread_barrier_wait(&scenario->solutionSetup_barrier);
                pthread_mutex_lock(&log->lock);
            }
            pthread_cond_wait(&log->read_cond, &log->lock);
        }

        if (scenario->state == COMPLETE && log->tail == log->head) {
            fclose(fp);
            pthread_exit(NULL);
        }
        // Write next string to file.
        fprintf(fp, "%s", log->data[log->tail]);

        //  Reset value
        memset(log->data[log->tail], '\0', sizeof(log->data[log->tail]));

        // Increment tail position, wrap if needed
        if (log->tail + 1 == log->size) {
            log->tail = 0;
        }
        else log->tail++;
        // log->tail = log->tail + 1 == log->size ? 0 : log->tail + 1;
        pthread_cond_broadcast(&log->write_cond);
        pthread_mutex_unlock(&log->lock);
    }
}

void log_destroy(log_queue_t *log) {
    pthread_mutex_destroy(&log->lock);
    pthread_cond_destroy(&log->read_cond);
    pthread_cond_destroy(&log->write_cond);
    return;
}
char * generateFileName(scenario_type scType) {
    static char fn[30];
    int prefLen =0;
    switch(scType) {
        case ROCK_1:
            prefLen = 4;
            memcpy(fn, "Rock", prefLen);
            break;
        case SINK_1:
            prefLen = 4;
            memcpy(fn, "Sink", prefLen);
            break;
        case FREE_1:
            prefLen = 4;
            memcpy(fn, "Free", prefLen);
            break;
        case MULTI:
            prefLen = 5;
            memcpy(fn, "Multi", prefLen);
            break;
        case FFA:
            prefLen = 3;
            memcpy(fn, "FFA", prefLen);
            break;
    }

    char timeText[17];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(timeText, sizeof(timeText), "%d-%m-%Y-%H:%M", t);

    memcpy(fn + prefLen, timeText, 16);
    memcpy(fn + prefLen + 16, ".txt", 4);
    return fn;
}
