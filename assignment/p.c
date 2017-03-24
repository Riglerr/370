#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define NUM_WHEELS 6
#define LOG_BUFF_LENGTH 10
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

typedef struct scenario_flags_t {
    unsigned int terminate : 1;
    unsigned int sinkThreadSetup : 1;
    unsigned int freeThreadSetup : 1;
    unsigned int blockThreadSetup : 1;
} scenario_flags_t;

typedef struct problem_conditions_t {
    pthread_cond_t sinking_condition;
    pthread_cond_t blocked_condition;
    pthread_cond_t freeWheeling_condition;
} problem_conditions_t;

typedef struct conditions_t {
    pthread_cond_t vector_condition;
    pthread_cond_t problem_condition;
} conditions_t;

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
        printf("SCENARIO COMPLETE.\n");

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

    printf("Starting solution threads...\n");
    pthread_t sinkT, blockT, freeT;
    pthread_create(&sinkT, NULL, sinkProblemHandler, (void *)scenario);
    pthread_create(&freeT, NULL, freeWheelProblemHandler, (void *)scenario);
    pthread_create(&blockT, NULL, blockProblemHandler, (void *)scenario);
    pthread_barrier_wait(&scenario->solutionSetup_barrier);

    pthread_mutex_lock(&scenario->mutex);
    scenario->state = VECTORING;
    pthread_mutex_unlock(&scenario->mutex);

    // Start VectorMonitor (Updates total distance travelled)
    printf("Starting VectorMonitor\n");
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
    struct timespec ts;
    ts.tv_nsec = 0;
    ts.tv_sec = 1;

    while(scenario->state != TERMINATE) {
        // Check if the scenario is complete.. make sure mutex is locke beforehand
        pthread_mutex_lock(&scenario->mutex);
        if (isScenarioComplete(scenario) == 1) {
            pthread_cond_broadcast(&scenario->scenarioComplete_condition);
            pthread_mutex_unlock(&scenario->mutex);
            pthread_exit(NULL);
        }

        // Pause if a problem is already being solved.
        // Get a new state for the wheel
        wheel->state = getNextWheelStateForScenario(scenario);

        while (scenario->state == PROBLEM) {
            printf("Wheel %d: Waiting for problems to be solved...\n", wheel->id);
            pthread_cond_wait(&scenario->vector_condition, &scenario->mutex);
        }

        switch(wheel->state) {
            case WORKING:
                printf("Wheel %d: Vectoring...\n", wheel->id);
                break;
            case SINKING:
                printf("Wheel %d: Sinking...\n", wheel->id);
                scenario->currentCycleProblems +=1;
                scenario->state = PROBLEM;

                // Add wheel index to problem queue
                pthread_cond_signal(&scenario->conditions.sinking_condition);
                while (wheel->state != WORKING && scenario->state != VECTORING) {
                    pthread_cond_wait(&scenario->vector_condition, &scenario->mutex);
                }
                break;
            case BLOCKED:
                printf("Wheel %d: Blocked...\n", wheel->id);
                scenario->currentCycleProblems += 1;
                scenario->state = PROBLEM;

                // Add wheel index to problem queue
                pthread_cond_signal(&scenario->conditions.blocked_condition);
                while (wheel->state != WORKING && scenario->state != VECTORING) {
                    pthread_cond_wait(&scenario->vector_condition, &scenario->mutex);
                }
                break;
            case FREEWHEELING:
                printf("Wheel %d: FreeWheeling...\n", wheel->id);
                scenario->currentCycleProblems +=1;
                scenario->state = PROBLEM;

                // Add wheel index to problem queue
                pthread_cond_signal(&scenario->conditions.freeWheeling_condition);
                while (wheel->state != WORKING && scenario->state != VECTORING) {
                    pthread_cond_wait(&scenario->vector_condition, &scenario->mutex);
                }
                break;
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
            pthread_cond_broadcast(&scenario->scenarioComplete_condition);
            pthread_mutex_unlock(&scenario->mutex);
            pthread_exit(NULL);
        }
        if (scenario->state == SETUP) {
            pthread_mutex_unlock(&scenario->mutex);
            pthread_barrier_wait(&scenario->solutionSetup_barrier);
            pthread_mutex_lock(&scenario->mutex);
        }

        printf("Sinkhandler: waiting for signal...\n");
        while(scenario->state != PROBLEM) {
            pthread_cond_wait(&scenario->conditions.sinking_condition, &scenario->mutex);
        }
        printf("SinkHandler: Signal Received, searching for problem\n");
        int x = 0;
        wheel_t *p;
        p = &scenario->wheels[0];
        // Deal with the first Sink problem
        for (int i =0; i < NUM_WHEELS; i ++) {
            if (p->state == SINKING) {
                x++;
                printf("SinkHandler: Resolving problem for wheel %d\n", p->id);

                // Solve the problem
                p->state = WORKING;
                x--;
                break;
            }
            p++;
        }
        if (x == 0)
            scenario->state = VECTORING;

        printf("Sinkhandler: signaling & releasing lock...\n");
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
            pthread_cond_broadcast(&scenario->scenarioComplete_condition);
            pthread_mutex_unlock(&scenario->mutex);
            pthread_exit(NULL);
        }
        if (scenario->state == SETUP) {
            pthread_mutex_unlock(&scenario->mutex);
            pthread_barrier_wait(&scenario->solutionSetup_barrier);
            pthread_mutex_lock(&scenario->mutex);
        }

        printf("BlockHandler: waiting for signal...\n");
        while(scenario->state != PROBLEM) {
            pthread_cond_wait(&scenario->conditions.blocked_condition, &scenario->mutex);
        }
        printf("BlockHandler: Signal Received, searching for problem\n");
        int x = 0;
        wheel_t *p;
        p = &scenario->wheels[0];
        // Deal with the first Sink problem
        for (int i =0; i < NUM_WHEELS; i ++) {
            if (p->state == BLOCKED) {
                x++;
                printf("BlockHandler: Resolving problem for wheel %d\n", p->id);

                // Solve the problem
                p->state = WORKING;
                x--;
                break;
            }
            p++;
        }
        if (x == 0)
            scenario->state = VECTORING;

        printf("BlockHandler: signaling & releasing lock...\n");
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
            pthread_cond_broadcast(&scenario->scenarioComplete_condition);
            pthread_mutex_unlock(&scenario->mutex);
            pthread_exit(NULL);
        }
        if (scenario->state == SETUP) {
            pthread_mutex_unlock(&scenario->mutex);
            pthread_barrier_wait(&scenario->solutionSetup_barrier);
            pthread_mutex_lock(&scenario->mutex);
        }

        printf("Freehandler: waiting for signal...\n");
        while(scenario->state != PROBLEM) {
            pthread_cond_wait(&scenario->conditions.freeWheeling_condition, &scenario->mutex);
        }
        printf("FreeHandler: Signal Received, searching for problem\n");
        int x = 0;
        wheel_t *p;
        p = &scenario->wheels[0];
        // Deal with the first problem
        for (int i =0; i < NUM_WHEELS; i ++) {
            if (p->state == FREEWHEELING) {
                x++;
                printf("FreeHandler: Resolving problem for wheel %d\n", p->id);

                // Solve the problem
                p->state = WORKING;
                x--;
                break;
            }
            p++;
        }
        if (x == 0)
            scenario->state = VECTORING;

        printf("Freehandler: signaling & releasing lock...\n");
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
    while(scenario->state != TERMINATE) {
        pthread_barrier_wait(&scenario->wheelCylce_barrier);
        pthread_mutex_lock(&scenario->mutex);
        scenario->totalDistanceVectored += 0.1;
        scenario->currentCycleProblems = 0;
        printf("TOTAL DISTANCE VECTORED: %f\n", scenario->totalDistanceVectored);
        printf("=============================\n");
        pthread_mutex_unlock(&scenario->mutex);
    }
    pthread_exit(NULL);
}
