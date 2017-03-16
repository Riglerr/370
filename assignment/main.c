#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

#define NSEC_MULTIPLIER 10000000000L
#define NUM_WHEELS 1

enum scenarios { NONE, SINGLE_ROCK, SINGLE_WHEEL_SINK, SINGLE_WHEEL_FREE, MULTIPLE_WHEEL};

typedef struct scenario_t{
    enum scenarios s_type;
} scenario_t;

typedef struct pstate_t {
    pthread_mutex_t mutex;
    pthread_cond_t resume_condition;
    pthread_cond_t interaction_setup_condition;
    pthread_cond_t log_setup_condition;
    pthread_cond_t scenario_setup_condition;
    int terminateFlag;
    int pauseFlag;
    int setupFlag;
    int numWheels;
    struct scenario_t currentScenario;
} pstate_t;

enum wheelState {
    working,
    blocked,
    freewheeling,
    sinking
};

typedef struct wheel {
    pstate_t *pState;
    int wheelId;
    enum wheelState state;
} wheel;

void* wheelController(void* args);
void* interactionManager(void *args);
void* startScenario(void *args);
void* pauseScenario(pstate_t *pState);
void* resumeScenario(pstate_t *pState);
void *terminateScenario(pstate_t *pState);
void *checkForInterrupt(pstate_t *pState);

void *interactionManager(void * args) {
    pstate_t *pState = (pstate_t*) args;
    printf("Starting InteractionManager ... \n");
    char keypress;
    printf("Interaction Manager started, waiting for keypress");
    for (;;) {
        scanf(" %c", &keypress);
        switch(keypress) {
            // p : Pause
            case 'p':
                pauseScenario(pState);
                break;
            // r: Resume
            case 'r':
                resumeScenario(pState);
                break;
            // t: Terminate
            case 't':
                terminateScenario(pState);
                pthread_exit(0);
            // q: Quit
            case 'q':
                exit(0);
            default:
                printf("%s%c%s","\nUnrecognised input ",keypress, " try again.\n");
                printf("Press 'p' to pause, 't' to terminate the current scenario, or 'q' to exit the program.");
                break;
        }
    }
}

void *logManager(void *args) {
    pstate_t *state = (pstate_t *)args;
    pthread_cond_broadcast(&state->log_setup_condition);
}

void *generateScenario(void *args) {
    pstate_t *state = (pstate_t *)args;
    pthread_cond_broadcast(&state->scenario_setup_condition);
}

void *checkForInterrupt(pstate_t *pState) {
    pthread_mutex_lock(&pState->mutex);
    while(pState->pauseFlag == 1 || pState->terminateFlag == 1) {
        if (pState->terminateFlag == 1) {
            pthread_mutex_unlock(&pState->mutex);
            pthread_exit(0);
        } else {
            pthread_cond_wait(&pState->resume_condition, &pState->mutex);
        }
    }
    pthread_mutex_unlock(&pState->mutex);
    return NULL;
}

void *pauseScenario(pstate_t *pState) {
    printf("PAUSING \n");
    pthread_mutex_lock(&pState->mutex);
    pState->pauseFlag = 1;
    pthread_mutex_unlock(&pState->mutex);
}

void *resumeScenario(pstate_t *pState) {
    pthread_mutex_lock(&pState->mutex);
    pState->pauseFlag = 0;
    pthread_cond_broadcast(&pState->resume_condition);
    pthread_mutex_unlock(&pState->mutex);
}

void *terminateScenario(pstate_t *pState) {
    printf("Terminating Scenario \n");
    pthread_mutex_lock(&pState->mutex);
    pState->terminateFlag = 1;
    pthread_cond_broadcast(&pState->resume_condition);
    pthread_mutex_unlock(&pState->mutex);
}

void *startScenario(void* args) {
    pstate_t *pState = (pstate_t*) args;
    pthread_t wheelThreads[6];
    pthread_t interactionThread, logThread, scenarioGenThread;
    struct wheel wheelArray[6];
    int t;
    int rc;
    void *exitStatus;

    // Start Interaction Manager
    pthread_create(&interactionThread, NULL, interactionManager, (void *)pState);

    // Create new Log file and keep it open.
    printf("Creating Log Files  ...\n");
    //pthread_create(&logThread, NULL, logManager, (void *)pState);

    // Wait for Setup to complete
//    pthread_mutex_lock(&pState->mutex);
//    while (pState->setupFlag == 0) {
//        pthread_cond_wait(&pState->interaction_setup_condition, &pState->mutex);
//        pthread_cond_wait(&pState->log_setup_condition, &pState->mutex);
//        pthread_cond_wait(&pState->scenario_setup_condition, &pState->mutex);
//        pState->setupFlag == 1;
//    }
//    pthread_mutex_unlock(&pState->mutex);

    // GO!
    printf("Creating Threads and Starting Scenario ...\n");
    printf("Press 'p' to pause the scenario at any time.\n");
    printf("Press 't' to terminate the scenario and return to the menu\n");
    printf("Press 'q' to quit the program.\n");
    for(t = 0; t < pState->numWheels; t++) {
        wheelArray[t].wheelId = t;
        wheelArray[t].pState = pState;
        rc = pthread_create(&wheelThreads[t], NULL, wheelController, (void *)&wheelArray[t]);
        if (rc) {
            printf("ERROR; return code from pthread_create() is %d\n", rc);
            pthread_exit(&rc);
        }
    }

    for(t = 0; t < pState->numWheels; t++) {
        rc = pthread_join(wheelThreads[t], &exitStatus);
        if (rc) {
            printf("ERROR; Failed to join thread, return code is %d\n", rc);
            pthread_exit(&rc);
        }
    }
    return NULL;
}

void *setupScenario(void *args) {

}

void *simulateWheel(void *args) {
    // lock the mutex
    // If program already in problem state, wait for ok condition.
    // Lock Mutex
    // Generate random input for scenario
    // If problem condition, signal problem state
    // Deal with problem
    // -> perform action, then randomly calculate success.
    // If problem solved, signal OK, release mutex.
    // If not solved after x5 tries, signal HALT
}

void * wheelController(void * args) {
    wheel *wheelData = (wheel *)args;
    pstate_t *pState = wheelData->pState;
    int i;
    struct timespec ts;
    struct timespec tr;
    ts.tv_sec = 1;
    ts.tv_nsec = 0;
    for (i = 0; i< 100; i++) {
        // Checks for pause / terminate conditions
        checkForInterrupt(pState);
        // checkProblemStateForWheel(wheelData);
        printf("%d: Output\n", wheelData->wheelId);
        nanosleep(&ts, &tr);
    }
   return NULL;
}


pstate_t* initialiseProgramState(pstate_t *pState, int wheelCount) {
    pthread_mutex_init(&pState->mutex, NULL);
    pthread_cond_init(&pState->resume_condition, NULL);
    pthread_cond_init(&pState->interaction_setup_condition, NULL);
    pthread_cond_init(&pState->log_setup_condition, NULL);
    pthread_cond_init(&pState->scenario_setup_condition, NULL);
    pState->terminateFlag = 0;
    pState->numWheels = wheelCount;
    pState->pauseFlag = 0;
    pState->setupFlag = 0;
    pState->currentScenario.s_type = NONE;
    return pState;
}

pstate_t* resetProgramState() {

}

int main() {
    char menuKeypress;
    int i = 0;
    pthread_t scenarioThread;
    pstate_t pState;

    // Main Loop.
    while (i == 0) {
        initialiseProgramState(&pState, 3);
        printf("\nSelect an option from the list below\n");
        printf("1. Scenario 1 - Single wheel Rock\n");
        printf("2. Scenario 2 - Single wheel sinking.\n");
        printf("3. Scenario 3 - Single wheel freewheeling.\n");
        printf("4. Scenario 4 - Multiple wheels encountering the same problem.\n");
        printf("Q. Exit\n");
        scanf(" %c", &menuKeypress);
        scenario_t cScen;
        switch (menuKeypress) {
            case '1':
                cScen.s_type = SINGLE_ROCK;
                pState.currentScenario = cScen;
                break;
            case '2':
                pState.currentScenario.s_type = SINGLE_WHEEL_SINK;
                break;
            case '3':
                pState.currentScenario.s_type = SINGLE_WHEEL_FREE;
                break;
            case '4':
                pState.currentScenario.s_type = MULTIPLE_WHEEL;
                break;
            case 'Q':
                i = 1;
                break;
            default:
                printf("%s%c%s","Unrecognised input ", menuKeypress, " try again. \n");
                continue;
        }
        pthread_create(&scenarioThread, NULL, startScenario, (void *)&pState);
        pthread_join(scenarioThread, NULL);
        printf("Finished Scenario.\n");
    }
    printf("\nPress any key to exit. \n");
    getchar();
    return 0;
}