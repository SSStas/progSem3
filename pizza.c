#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <assert.h>
#include <sys/shm.h>
#include <sys/sem.h>


typedef struct pair {
    char letter;
    int count;
} pair;

typedef struct data {
    int wordsOffset;
    char currentWords[5][6];
    char *conveyor[5];
    int ingredients[4];
    int count;
    int correctResult;
    int startWordCount;
    int maxCount;
} data;

void unlockCooks(int semId) {
    // cook 'P', cook 'I', cook 'Z', cook 'A', controller
    struct sembuf sops[5] = {{0, 1, 0}, {1, 1, 0}, {2, 2, 0}, {3, 1, 0}, {4, 5, 0}};

    if (semop(semId, sops, 5) == -1) {
        perror("shm:unlockCooks");
    }
}

void lockCook(int semId, int semNum) {
    struct sembuf sops = {semNum, -1, 0};

    if (semop(semId, &sops, 1) == -1) {
        perror("shm:lockCook");
    }
}

void waitProcess(int semId, int semNum) {
    struct sembuf sops = {semNum, 0, 0};

    if (semop(semId, &sops, 1) == -1) {
        perror("shm:waitProcess");
    }
}

void unlockController(int semId) {
    struct sembuf sops = {4, -1, 0};

    if (semop(semId, &sops, 1) == -1) {
        perror("shm:lockCook");
    }
}

void unlockAssistant(int semId) {
    struct sembuf sops = {5, 1, 0};

    if (semop(semId, &sops, 1) == -1) {
        perror("shm:unlockAssistant");
    }
}

void lockAssistant(int semId) {
    struct sembuf sops = {5, -1, 0};

    if (semop(semId, &sops, 1) == -1) {
        perror("shm:lockAssistant");
    }
}

void moveConveyor(data *processRes) {
    char *lastWord = NULL;
    processRes->wordsOffset = (processRes->wordsOffset + 1) % 5;

    if (processRes->conveyor[4] != NULL) {
        processRes->conveyor[4][0] = '\0';
    }
    
    if (processRes->startWordCount < processRes->maxCount) {
        processRes->conveyor[4] = processRes->currentWords[processRes->wordsOffset];
        lastWord = processRes->conveyor[4];
        processRes->startWordCount++;
    }

    for (int i = 4; i > 0; --i) {
        processRes->conveyor[i] = processRes->conveyor[i - 1]; 
    }

    processRes->conveyor[0] = lastWord;
}

void cook(int semId, int semNum, char letter, data *processRes) {
    int semVal = 0;

    while (1) {
        lockCook(semId, semNum);

        if (processRes->count == processRes->maxCount) {
            return;
        }

        if (processRes->ingredients[semNum] == 0) {
            semVal = semctl(semId, semNum, GETVAL, NULL) + 1;
            semctl(semId, semNum, SETVAL, 1);
            processRes->ingredients[semNum] = -1;
            unlockAssistant(semId);
            waitProcess(semId, semNum);
            semctl(semId, semNum, SETVAL, semVal);
            continue;
        }

        if (processRes->conveyor[semNum] != NULL) {
            processRes->ingredients[semNum]--;
            strncat(processRes->conveyor[semNum], &letter, 1);
            printf("Cook %d add %c (left: %d ingr.)\n", 
                semNum, letter, processRes->ingredients[semNum]);
        } else {
            printf("Cook %d did not add %c (left: %d ingr.)\n", 
                semNum, letter, processRes->ingredients[semNum]);
        }

        unlockController(semId);
    }
}

void controller(int semId, data *processRes, char *fullWord) {
    while (1) {
        waitProcess(semId, 4);

        printf("\nConveyor view:\n");
        for (int i = 0; i < 4; ++i) {
            if (processRes->conveyor[i] != NULL) {
                printf("Cook %d: %s\n", i, processRes->conveyor[i]);
            } else {
                printf("Cook %d: (NULL)\n", i);
            }
        }

        moveConveyor(processRes);

        if (processRes->conveyor[4] != NULL) {
            printf("Controller checked! Result: %s\n", processRes->conveyor[4]);
        } else {
            printf("Controller checked! Result: (NULL)\n");
        }

        printf("---\n");
        
        if (processRes->conveyor[4] != NULL) {
            if (!strcmp(processRes->conveyor[4], fullWord)) {
                processRes->correctResult++;
            }
            processRes->conveyor[4][0] = '\0';
            processRes->conveyor[4] = NULL;
            processRes->count++;
        }

        unlockCooks(semId);

        if (processRes->count == processRes->maxCount) {
            unlockAssistant(semId);
            printf("Total: %d; Good pizza: %d; Bad pizza: %d\n", processRes->count, 
                processRes->correctResult, processRes->count - processRes->correctResult);
            return;
        }
    }
}

void assistant(int semId, data *processRes, int numOfIngedients) {
    while (1) {
        lockAssistant(semId);

        if (processRes->count == processRes->maxCount) {
            return;
        }

        for (int i = 0; i < 4; ++i) {
            if (processRes->ingredients[i] == -1) {
                processRes->ingredients[i] = numOfIngedients;
                printf("Assistant give %d ingredients to Cook %d\n", numOfIngedients, i);
                semctl(semId, i, SETVAL, 0);
                break;
            }
        }
    }
}

int main(int argc, char **argv) {
    int shmId, semId, maxCount = 10, numOfIngedients = 10;
    
    pair letters[] = {{'P', 1}, {'I', 1}, {'Z', 2}, {'A', 1}};
    char fullWord[] = "PIZZA";
    int countProc = (sizeof(letters) / sizeof(pair)) + 2;

    if (argc == 2) {
        maxCount = atoi(argv[1]);
    } else if (argc != 1) {
        printf("pizza: arguments error\n");
        return 0;
    } 

    assert(maxCount > 0);

    shmId = shmget(IPC_PRIVATE, sizeof(data), 0666 | IPC_CREAT);
    semId = semget(IPC_PRIVATE, countProc, 0666 | IPC_CREAT);

    data *processRes = shmat(shmId, NULL, 0666);
    for (int i = 0; i < 5; ++i) {
        processRes->currentWords[i][0] = '\0';
        processRes->conveyor[i] = NULL;
    }
    for (int i = 0; i < 4; ++i) {
        processRes->ingredients[i] = numOfIngedients;
    }
    processRes->count = 0;
    processRes->wordsOffset = 4;
    processRes->correctResult = 0;
    processRes->maxCount = maxCount;
    processRes->startWordCount = 0;

    unlockCooks(semId);
    moveConveyor(processRes);

    for (int i = 0; i < countProc - 2; ++i) {
        pid_t pid = fork();
        if (pid == 0) {
            cook(semId, i, letters[i].letter, processRes);
            exit(0);
        }
    }

    pid_t pid = fork();
    if (pid == 0) {
        controller(semId, processRes, fullWord);
        exit(0);
    }

    pid = fork();
    if (pid == 0) {
        assistant(semId, processRes, numOfIngedients);
        exit(0);
    }

    for (int i = 0; i < countProc; ++i) {
        wait(NULL);
    }

    if (shmctl(shmId, IPC_RMID, NULL) == -1) {
        perror("pizza:shmctl");
    }
    if (semctl(semId, 0, IPC_RMID, NULL) == -1) {
        perror("pizza:semctl");
    }

    return 0;
}

