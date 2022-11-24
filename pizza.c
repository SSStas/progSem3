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
    char currentWord[10];
    int count;
    int maxCount;
    int correctResult;
} data;

void cooker(int semId, struct sembuf *sops, char letter, data *processRes) {
    while (1) {
        // lock
        if (semop(semId, sops, 1) == -1) {
            perror("shm:semop");
        }

        strncat(processRes->currentWord, &letter, 1);
        printf("Cooker add %c\n", letter);

        if (semctl(semId, sops[0].sem_num, GETVAL, NULL) == 0) {
            // unlock next
            if (semop(semId, sops + 1, 1) == -1) {
                perror("shm:semop");
            }

            if (processRes->count + 1 >= processRes->maxCount) {
                return;
            }
        }
    }
}

void controller(int semId, struct sembuf *sops, data *processRes, char *fullWord) {
    while (1) {
        // lock
        if (semop(semId, sops, 1) == -1) {
            perror("shm:semop");
        }

        printf("Controller checked! Result: %s\n", processRes->currentWord);
        
        if (!strcmp(processRes->currentWord, fullWord)) {
            processRes->correctResult++;
        }
        processRes->currentWord[0] = '\0';
        processRes->count++;


        if (processRes->count == processRes->maxCount) {
            printf("Total: %d; Good pizza: %d; Bad pizza: %d\n", processRes->count, 
                processRes->correctResult, processRes->count - processRes->correctResult);
            return;
        }

        // unlock next
        if (semop(semId, sops + 1, 1) == -1) {
            perror("shm:semop");
        }
    }
}

int main(int argc, char **argv) {
    int shmId, semId,  index, maxCount = 10;
    
    pair letters[] = {{'P', 1}, {'I', 1}, {'Z', 2}, {'A', 1}};
    char fullWord[] = "PIZZA";
    int countProc = (sizeof(letters) / sizeof(pair)) + 1;
    struct sembuf sops[countProc][2];

    if (argc == 2) {
        maxCount = atoi(argv[1]);
    } else if (argc != 1) {
        printf("pizza: arguments error\n");
        return 0;
    } 

    assert(maxCount > 0);

    for (int i = 0; i < countProc; ++i) {
        sops[i][0].sem_num = i;
        sops[i][0].sem_op = -1;
        sops[i][0].sem_flg = 0;

        index = (i + 1) % countProc;
        sops[i][1].sem_num = index;
        sops[i][1].sem_op = (index== countProc - 1) ? 1 : letters[index].count;
        sops[i][1].sem_flg = 0;
    }

    shmId = shmget(IPC_PRIVATE, sizeof(data), 0666 | IPC_CREAT);
    semId = semget(IPC_PRIVATE, countProc, 0666 | IPC_CREAT);

    if (semop(semId, &(sops[countProc - 1][1]), 1) == -1) {
        perror("shm:semop");
    }

    data *processRes = shmat(shmId, NULL, 0666);
    processRes->currentWord[0] = '\0';
    processRes->count = 0;
    processRes->maxCount = maxCount;
    processRes->correctResult = 0;

    for (int i = 0; i < countProc - 1; ++i) {
        pid_t pid = fork();
        if (pid == 0) {
            cooker(semId, sops[i], letters[i].letter, processRes);
            exit(0);
        }
    }

    pid_t pid = fork();
    if (pid == 0) {
        controller(semId, sops[countProc - 1], processRes, fullWord);
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

