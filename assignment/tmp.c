#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define NUM_WHEELS 6
#define MIN_PROBLEMS_PER_SCENARIO 5
#define PROBLEM_RETRY_ATTEMPTS 3
#define MIN_VECTOR_DISTANCE 1
#define FAILURE_PROBABILITY 20
#define BUFF_H 4
#define BUFF_W 256

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
    COMPLETE
} scenario_state;

typedef enum scenario_type {
    ROCK_1,
    SINK_1,
    FREE_1,
    MULTI,
    FFA
} scenario_type;

typedef enum scenario_outcome {
    PASSED,
    FAILED
} scenario_outcome;


typedef struct shared_buffer {
    pthread_mutex_t lock;
    pthread_cond_t
            new_data_cond,
            new_space_cond;
    char c[BUFF_H][BUFF_W];
    int next_in,
            next_out,
            count;
    int close;
    char *fileName;
} shared_buffer_t;

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
    pthread_cond_t continue_condition;
    pthread_cond_t problem_condition;
    pthread_cond_t scenarioComplete_condition;
    problem_conditions_t conditions;
    pthread_barrier_t wheelSetup_barrier;
    pthread_barrier_t solutionSetup_barrier;
    pthread_barrier_t wheelCycle_barrier;
    shared_buffer_t log;
    scenario_outcome outcome;
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
int trySolveProblem(scenario_t *scenario, wheel_t * wheel, wheel_state pType);
void *sinkProblemHandler(void * args);
void *freeWheelProblemHandler(void * args);
void *blockProblemHandler(void * args);
void *scenarioMonitor(void *p_scenario);
wheel_state randomizeSingleState(wheel_state secondaryState);
wheel_state randomizeStateForScenario(scenario_t *scenario);

shared_buffer_t log_init(scenario_type scType);
void log_destroy(shared_buffer_t *sb);
void *log_consume(void *args);
void *log_print(shared_buffer_t *sb, char string[]);

char* generateFileName(scenario_type scType);
char *getLogNameForProblemType(wheel_state pType);

int processWheelState(wheel_t *wheel, scenario_t *scenario);
void waitForContinueSignal(wheel_t *wheel, scenario_t *scenario);

/*
 * main
 * Shows menu, handles input & starts scenarios*/
int main() {
    srand((unsigned)time(NULL));
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
        printf("4. Scenario 4 - Multiple Wheel encountering multiple problems.\n");
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
    pthread_cond_init(&scenario.continue_condition, NULL);
    pthread_cond_init(&scenario.scenarioComplete_condition, NULL);
    pthread_barrier_init(&scenario.wheelSetup_barrier, NULL, NUM_WHEELS);
    pthread_barrier_init(&scenario.solutionSetup_barrier, NULL, 4);
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
    pthread_create(&fLoggerThread, NULL, log_consume, (void *)&scenario->log);
    printf("Logging to: %s\n", scenario->log.fileName);

    log_print(&scenario->log, "Starting Solution Threads\n");
    pthread_create(&sinkT, NULL, sinkProblemHandler, (void *)scenario);
    pthread_create(&freeT, NULL, freeWheelProblemHandler, (void *)scenario);
    pthread_create(&blockT, NULL, blockProblemHandler, (void *)scenario);

    // Wait for solution handlers to be ready.
    pthread_barrier_wait(&scenario->solutionSetup_barrier);

    pthread_mutex_lock(&scenario->mutex);
    scenario->state = VECTORING;
    pthread_mutex_unlock(&scenario->mutex);

    // Start VectorMonitor (Updates total distance travelled)
    log_print(&scenario->log, "Starting VectorMonitor\n");
    pthread_create(&vMT, NULL, scenarioMonitor, (void *)scenario);

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
    while(scenario->state != COMPLETE) {
        pthread_cond_wait(&scenario->scenarioComplete_condition, &scenario->mutex);
        pthread_mutex_unlock(&scenario->mutex);
    }
    log_print(&scenario->log, "SCENARIO_COMPLETED\n");
    for (int i = 0; i < NUM_WHEELS; i++) {
        pthread_join(wheelThreads[i], NULL);
    }
    // Ensure all threads are destroyed before exiting this scenario.
    // Signal the problem handler threads once more
    // This allows them to exit gracefully.
    pthread_cond_signal(&scenario->conditions.blocked_condition);
    pthread_cond_signal(&scenario->conditions.sinking_condition);
    pthread_cond_signal(&scenario->conditions.freeWheeling_condition);

    //printf("Attemping to close threads\n");
   // printf("Waiting for sinker to end\n");
    pthread_join(sinkT, NULL);
    //printf("Waiting for blockH to end\n");
    pthread_join(blockT, NULL);
   // printf("Waiting for freeH to end\n");
    pthread_join(freeT, NULL);

    scenario->log.close = 1;
    pthread_cond_broadcast(&scenario->log.new_data_cond);
    //printf("Waiting for logger to end\n");
    pthread_join(fLoggerThread, NULL);

   // printf("Waiting for scenMon to end\n");
    pthread_join(vMT, NULL);
    return 0;
}

void scenario_destroy(scenario_t *scenario) {

    // Free pthread structs
    pthread_mutex_destroy(&scenario->mutex);
    pthread_cond_destroy(&scenario->problem_condition);
    pthread_cond_destroy(&scenario->continue_condition);
    pthread_cond_destroy(&scenario->scenarioComplete_condition);
    pthread_cond_destroy(&scenario->conditions.sinking_condition);
    pthread_cond_destroy(&scenario->conditions.freeWheeling_condition);
    pthread_cond_destroy(&scenario->conditions.blocked_condition);
    pthread_barrier_destroy(&scenario->wheelSetup_barrier);
    pthread_barrier_destroy(&scenario->solutionSetup_barrier);
    pthread_barrier_destroy(&scenario->wheelCycle_barrier);

    log_destroy(&scenario->log);
    return;
}

void *wheel_start(void *args) {
    scenario_wheel_t *threadData = (scenario_wheel_t *)args;
    scenario_t *scenario = threadData->scenario;
    wheel_t *wheel = threadData->wheel;
    shared_buffer_t *log = &scenario->log;
    struct timespec ts;
    ts.tv_nsec = 0;
    ts.tv_sec = 1;
    char msg[255];
    // Synchronize first wheel run.
    pthread_barrier_wait(&scenario->wheelSetup_barrier);
    while(1) {
        if(scenario->state == COMPLETE) {
            break;
        }
        pthread_mutex_lock(&scenario->mutex);
        wheel->state = randomizeStateForScenario(scenario);
        // Block while another problem is being solved.
        waitForContinueSignal(wheel, scenario);

        if (isScenarioComplete(scenario) == 0) { // Exit thread if complete.
            // Vector or Signal problem.
            processWheelState(wheel, scenario);

            while (wheel->state != WORKING && scenario->state != VECTORING && scenario->state != COMPLETE) {
                pthread_cond_wait(&scenario->continue_condition, &scenario->mutex);
            }
        }
//        if(scenario->state == COMPLETE) {
//            pthread_mutex_unlock(&scenario->mutex);
//            break;
//        }
        pthread_mutex_unlock(&scenario->mutex);
        pthread_barrier_wait(&scenario->wheelCycle_barrier);
        nanosleep(&ts,NULL); // Sleep for 1 Sec before continuing.

    }
    log_print(log, msg);
    pthread_exit(NULL);
    return NULL;
}

void waitForContinueSignal(wheel_t *wheel, scenario_t *scenario) {
    shared_buffer_t *log = &scenario->log;
    char msg[255];
    while (scenario->state == PROBLEM & scenario->state != COMPLETE) {
        sprintf(msg, "Wheel %d: Waiting for problems to be solved...\n", wheel->id);
        log_print(log, msg);
        pthread_cond_wait(&scenario->continue_condition, &scenario->mutex);
    }
}

int processWheelState(wheel_t *wheel, scenario_t *scenario) {
    shared_buffer_t *log = &scenario->log;
    char msg[255];
    switch(wheel->state) {
        case WORKING:
            sprintf(msg, "Wheel %d: Vectoring...\n", wheel->id);
            log_print(log, msg);
            return 0;
        case SINKING:
            sprintf(msg, "Wheel %d: Sinking...\n", wheel->id);
            log_print(log, msg);
            scenario->currentCycleProblems +=1;
            scenario->state = PROBLEM;
            pthread_cond_signal(&scenario->conditions.sinking_condition);
            return 1;
        case BLOCKED:
            sprintf(msg, "Wheel %d: Blocked...\n", wheel->id);
            log_print(log, msg);
            scenario->currentCycleProblems += 1;
            scenario->state = PROBLEM;
            pthread_cond_signal(&scenario->conditions.blocked_condition);
            return 2;
            break;
        case FREEWHEELING:
            sprintf(msg, "Wheel %d: FreeWheeling...\n", wheel->id);
            log_print(log, msg);
            scenario->currentCycleProblems +=1;
            scenario->state = PROBLEM;
            pthread_cond_signal(&scenario->conditions.freeWheeling_condition);
            return 3;
    }
}

void *sinkProblemHandler(void *args) {
    scenario_t *scenario = (scenario_t *) args;
    pthread_mutex_t *mutex = &scenario->mutex;
    while (scenario->state != COMPLETE) {
        pthread_mutex_lock(&scenario->mutex);
        if (isScenarioComplete(scenario) == 1) {
            printf("BlockHandler: Exiting\n");
            pthread_mutex_unlock(mutex);
            break;
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

        log_print(&scenario->log, "SinkHandler: Signal Received...\n");
        for (int i = 0; i < NUM_WHEELS; i++) {
            int result = trySolveProblem(scenario, &scenario->wheels[i], SINKING);
            if (result == 1) {
                log_print(&scenario->log, "Terminating Scenario...\n");
                printf("FreeHandler: Exiting\n");
                scenario->outcome = FAILED;
                break;
            }
        }
        scenario->state = scenario->outcome == FAILED ? COMPLETE: VECTORING;
        log_print(&scenario->log, "SinkHandler: signaling & releasing lock...\n");
        pthread_cond_broadcast(&scenario->continue_condition);
        pthread_mutex_unlock(&scenario->mutex);

    }
    pthread_exit(NULL);
}

void *blockProblemHandler(void *args) {
    scenario_t *scenario = (scenario_t *) args;
    while (scenario->state != COMPLETE) {
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
        for (int i = 0; i < NUM_WHEELS; i++) {
            int result = trySolveProblem(scenario, &scenario->wheels[i], BLOCKED);
            if (result == 1) {
                log_print(&scenario->log, "Terminating Scenario...\n");
                scenario->state = COMPLETE;
                scenario->outcome = FAILED;
                break;
            }
        }
        scenario->state = scenario->outcome == FAILED ? COMPLETE: VECTORING;
        log_print(&scenario->log, "BlockHandler: signaling & releasing lock...\n");
        pthread_cond_broadcast(&scenario->continue_condition);
        pthread_mutex_unlock(&scenario->mutex);

    }
    pthread_exit(NULL);
}

void *freeWheelProblemHandler(void * args) {
    scenario_t *scenario = (scenario_t *) args;
    while (scenario->state != COMPLETE) {
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
        while(scenario->state != PROBLEM) {
            pthread_cond_wait(&scenario->conditions.freeWheeling_condition, &scenario->mutex);
            if (scenario->state == COMPLETE) {
                pthread_mutex_unlock(&scenario->mutex);
                pthread_exit(NULL);
            }
        }
        log_print(&scenario->log, "FreeHandler: Signal Received, searching for problem\n");
        for (int i = 0; i < NUM_WHEELS; i++) {
            int result = trySolveProblem(scenario, &scenario->wheels[i], FREEWHEELING);
            if (result == 1) {
                log_print(&scenario->log, "Terminating Scenario...\n");
                scenario->state = COMPLETE;
                scenario->outcome = FAILED;
                break;
            }
        }
        scenario->state = scenario->outcome == FAILED ? COMPLETE: VECTORING;
        log_print(&scenario->log, "Freehandler: signaling & releasing lock...\n");
        pthread_cond_broadcast(&scenario->continue_condition);
        pthread_mutex_unlock(&scenario->mutex);

    }
    pthread_exit(NULL);
}

int trySolveProblem(scenario_t *scenario, wheel_t * wheel, wheel_state pType) {
    shared_buffer_t *log = &scenario->log;
    int attempts = 0;
    char msg[255];
    char *logName = getLogNameForProblemType(pType);
    int rando_calrissian;
    if (wheel->state == pType) {
        sprintf(msg, "%s: Resolving problem for wheel %d\n", logName, wheel->id);
        log_print(&scenario->log, msg);
        while(wheel->state != WORKING && attempts < PROBLEM_RETRY_ATTEMPTS) {
            rando_calrissian = rand() % 100;
            if (rando_calrissian >= FAILURE_PROBABILITY) {
                wheel->state = WORKING;
                sprintf(msg, "%s: Problem Solved\n", logName);
                log_print(log, msg);
                return 0;
            }
            else {
                sprintf(msg, "%s: Failed to solved problem, attempt: %d\n", logName, attempts + 1);
                log_print(&scenario->log, msg);
                attempts++;
            }
        }

        // Failed to solve problem 3 times.
        sprintf(msg, "%s: Failed to solve problem after 3 Attempts...\n", logName);
        log_print(log, msg);
        return 1;
    }
    return 0;
}

char *getLogNameForProblemType(wheel_state pType) {
    static char name[20];
    switch (pType) {
        case SINKING:
            strcpy(name, "SinkHandler");
            break;
        case FREEWHEELING:
            strcpy(name, "FreeHandler");
            break;
        case BLOCKED:
            strcpy(name, "BlockHandler");
            break;
    }
    return name;
}

int isScenarioComplete(scenario_t *scenario) {
    if (scenario->solvedProblemCount == MIN_PROBLEMS_PER_SCENARIO
        || scenario->totalDistanceVectored >= MIN_VECTOR_DISTANCE || scenario->state == COMPLETE) {
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

/*
 * Function: scenarioMonitor
 * --------------------------
 * Checks the Scenario state after each wheel cycle,
 * It increments the total Distance Vectored each Iteration.
 * Responsible for Signalling that the scenario has completed.
 *
 * p_scenario: Pointer to a scenario struct.
 *
 * returns: NULL
 * pthread_exit status: 0
 * */
void *scenarioMonitor(void *p_scenario) {
    scenario_t *scenario = (scenario_t *)p_scenario;
    pthread_mutex_t *mutex = &scenario->mutex;
    shared_buffer_t *log = &scenario->log;
    char msg[255];
    while(1) {
        //sprintf(msg, "SMon: Waiting at barrier\n");
        //log_print(log, msg);
        pthread_barrier_wait(&scenario->wheelCycle_barrier);
        pthread_mutex_lock(mutex);
        sprintf(msg, "TOTAL DISTANCE VECTORED: %f\n", scenario->totalDistanceVectored);
        log_print(log, "==========================\n");
        log_print(log, msg);
        log_print(log, "==========================\n");
        scenario->totalDistanceVectored += 0.1;
        scenario->currentCycleProblems = 0;
        scenario->multiReset = 0;
        if (isScenarioComplete(scenario) == 1) {
            if (scenario->state != COMPLETE) {
                scenario->state = COMPLETE;
            }
            if (scenario->outcome != FAILED) {
                scenario->outcome = PASSED;
            }
            pthread_cond_broadcast(&scenario->scenarioComplete_condition);
            pthread_mutex_unlock(mutex);
            break;
        }
        pthread_mutex_unlock(mutex);
    }
    // printf("SCMON: Exiting");
    pthread_exit(0);
}

shared_buffer_t log_init(scenario_type scType) {
    shared_buffer_t sb;
    sb.next_in = sb.next_out = sb.count = 0;
    sb.close = 0;
    pthread_mutex_init(&sb.lock, NULL);
    pthread_cond_init(&sb.new_data_cond, NULL);
    pthread_cond_init(&sb.new_space_cond, NULL);

    // Gen FileName for scenario
    sb.fileName = generateFileName(scType);

    return sb;
}

void log_destroy(shared_buffer_t *sb) {
    pthread_mutex_destroy(&sb->lock);
    pthread_cond_destroy(&sb->new_data_cond);
    pthread_cond_destroy(&sb->new_space_cond);
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
    }

    char timeText[17];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(timeText, sizeof(timeText), "%d-%m-%Y-%H:%M", t);

    memcpy(fn + prefLen, timeText, 16);
    memcpy(fn + prefLen + 16, ".txt", 4);
    return fn;
}

/* producer is intended to be the body of a thread.
   It repeatedly reads the next line of text from the standard input,
   and puts it into the buffer pointed to by the parameter sb.
   It terminates when it encounters an EOF character in input.
   The EOF character is passed on to the consumer.
 */
void *log_print(shared_buffer_t *sb, char string[]) {
    printf("%s", string);
    pthread_mutex_lock(&sb->lock);
    // Wait for space
    while (sb->count == BUFF_H)
        pthread_cond_wait(&sb->new_space_cond, &sb->lock);

    // Copy string in buffer
    strcpy(sb->c[sb->next_in], string);
    sb->count++;
    sb->next_in = (sb->next_in + 1) % BUFF_H;
    pthread_mutex_unlock(&sb->lock);
    pthread_cond_signal(&sb->new_data_cond);
    return NULL;
}

void *log_consume(void *args) {
    shared_buffer_t *sb = (shared_buffer_t *)args;
    FILE *fp;
    fp = fopen(sb->fileName,"a");
    if (fp == NULL) {
        perror( "Error opening file" );
        printf( "Error code opening file: %d\n", errno );
        printf( "Error opening file: %s\n", strerror( errno ) );
        exit(-1);
    }
    for (;;) {
        pthread_mutex_lock(&sb->lock);
        while(sb->count == 0 && sb->close == 0)
            pthread_cond_wait(&sb->new_data_cond, &sb->lock);

        if (sb->close == 1) {
            fclose(fp);
            pthread_exit(NULL);
        }
        pthread_mutex_unlock(&sb->lock);
        fprintf(fp, "%s", sb->c[sb->next_out]);
        memset(sb->c[sb->next_out], '\0', sizeof(sb->c[sb->next_out]));
        pthread_mutex_lock(&sb->lock);
        sb->next_out = (sb->next_out + 1) % BUFF_H;
        sb->count--;
        pthread_mutex_unlock(&sb->lock);
        pthread_cond_signal(&sb->new_space_cond);
    }
}
