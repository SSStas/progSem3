#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/time.h>


static int ARG_MAX = 2048;
static int LEN_MAX = 32768;

int getWordsArr(char *str, char *sep, char **arr, int maxSize) {
    int count = 0;
    char *istr = strtok(str, sep);
    if (count >= maxSize) {
        return count;
    }

    while (istr != NULL) {
        arr[count++] = istr;

        istr = strtok(NULL, sep);
        if (count >= maxSize) {
            return count;
        }
    }

    return count;
}

int getProcArgs(char *str, int strLength, char **arr, int maxSize) {
    int count = 0, isCurrentSpace = 0, isLastSpace = 1;
    for (int i = 0; i < strLength; ++i) {
        isCurrentSpace = isspace(str[i]);

        if (isLastSpace && !isCurrentSpace) {
            if (count + 1 >= maxSize) {
                arr[count] = NULL;
                return maxSize;
            } else {
                arr[count] = (str + i);
            }
            ++count;
        }

        if (isCurrentSpace) {
            str[i] = '\0';
        }

        isLastSpace = isCurrentSpace;
    }

    arr[count] = NULL;

    return ++count;
}

int startNewProcess(int countProc, int maxCountProc, char **args, int *lastFd) {
    int fd[2];
    
    // start pipes
    if (pipe(fd) == -1) {
        perror("myWc");
        return -1;
    }

    pid_t pid = fork();

    // child process
    if (pid == 0) {
        if (*lastFd != -1) {
            dup2(*lastFd, 0);
            close(*lastFd);
        }

        if (countProc + 1 < maxCountProc) {
            close(fd[0]);
            dup2(fd[1], 1);
            close(fd[1]);
        }
        
        if (execvp(args[0], args) == -1) {
            printf("myShell: Command \'%s\' not found\n", args[0]);
            exit(-1);
        } 
    } else {
        if (*lastFd != -1) {
            close(*lastFd);
        }

        close(fd[1]);

        *lastFd = fd[0];
    }

    return 1;
}

char *cutText(char *str) {
    char *res = NULL;
    int index = 0;
    while (str[index] != '\0') {
        if (!isspace(str[index])) {
            res = (str + index);
            break;
        }
        ++index;
    }

    if (str[index] == '\0') {
        return res;
    }

    index = strlen(str) - 1;
    while (index >= 0) {
        if (!isspace(str[index])) {
            str[index + 1] = '\0';
            break;
        }
        --index;
    }

    return res;
}

int startSeveralProcesses(int cmdCount, char **args, int *processCount) {
    char *arguments[ARG_MAX];
    int lastFd = -1;

    for (int i = 0; i < cmdCount; ++i) {
        args[i] = cutText(args[i]);

        if (args[i] == NULL) {
            printf("myShell: Syntax error near unexpected token \'|\'\n");
            return -1;
        }

        getProcArgs(args[i], strlen(args[i]), arguments, ARG_MAX);

        if (startNewProcess(i, cmdCount, arguments, &lastFd) == -1) {
            return -1;
        }
        ++(*processCount);
    }

    return 0;
}

int isFirstSep(char *str, int lastLen, int newLen) {
    int index = lastLen;
    while (index <= newLen) {
        if (str[index] == '|') {
            return 1;
        } else if (!isspace(str[index])) {
            return 0;
        }
        ++index;
    }
    return 0;
}

int isLastSep(char *str, int len) {
    int index = len - 1;
    while (index >= 0) {
        if (str[index] == '|') {
            return 1;
        } else if (!isspace(str[index])) {
            return 0;
        }
        --index;
    }
    return 0;
}

int isSymText(char *str, int len) {
    int index = 0;
    while (index < len) {
        if (!isspace(str[index])) {
            return 1;
        }
        ++index;
    }
    return 0;
}

int main(int argc, char **argv) {
    int wordsCount = 0, len = 0, lastLen = 0, processCount = 0;
    char *textLines = malloc(LEN_MAX);
    char *words[ARG_MAX];

    if (argc > 1) {
        printf("myShell: Argument's error\n");
        return 0;
    }
    
    // get full multiline text
    while (1) {
        printf("> ");

        lastLen = len;

        if (fgets(textLines + len, LEN_MAX - len, stdin) == NULL) { 
            printf("myShell: Argument's reading error\n");
            return 0;
        }

        len = strlen(textLines);

        if (isFirstSep(textLines, lastLen, len)) {
            printf("myShell: Syntax error near unexpected token \'|\'\n");
            free(textLines);
            return 0;
        }

        if (isSymText(textLines, len) && (!isLastSep(textLines, len) || len + 1 >= LEN_MAX)) {
            break;
        }
    }
    
    // double token '|' processing (truncation)
    for (int i = 0; i < len; ++i) {
        if (i + 1 < len && textLines[i] == '|' && textLines[i] == textLines[i + 1]) {
            textLines[i] = '\0';
            break;
        }
    }

    wordsCount = getWordsArr(textLines, "|", words, ARG_MAX);

    if (startSeveralProcesses(wordsCount, words, &processCount) == -1) {
        free(textLines);
        exit(-1);
    }
    
    for (int i = 0; i < processCount; ++i) {
        wait(NULL);
    }

    free(textLines);

    return 0;
}
