#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>


typedef struct Parameters_t {
    int iParam;
    int fParam;
    int vParam;
} Parameters_t;

int isDirExist(const char *folder) {
    assert(folder != NULL);

    struct stat sb;
    return (stat(folder, &sb) == 0 && S_ISDIR(sb.st_mode));
}

char *getNewFilePath(char *folder, char *fileName) {
    assert(folder != NULL && fileName != NULL);

    char *startFileName = fileName;
    int index = strlen(fileName) - 1;
    while(index >= 0) {
        if (index >= 1 && fileName[index - 1] == '/') {
            startFileName = (fileName + index);
            break;
        }
        --index;
    }
    
    if (folder[strlen(folder) - 1] != '/') {
        return strcat(strcat(folder, "/"), startFileName);
    }

    return strcat(folder, startFileName);
}

int isFilesSame(const char *filePath1, const char *filePath2) {
    assert(filePath1 != NULL && filePath2 != NULL);

    char fullPath1[PATH_MAX], fullPath2[PATH_MAX];
    char *ptr1 = realpath(filePath1, fullPath1);

    if (ptr1 == NULL || ptr1 != fullPath1) {
        if (errno == ENOENT) {
            return 0;
        }
        return -1;
    }

    char *ptr2 = realpath(filePath2, fullPath2);

    if (ptr2 == NULL || ptr2 != fullPath2) {
        if (errno == ENOENT) {
            return 0;
        }
        return -1;
    }
    
    return !strcmp(ptr1, ptr2);
}

int overwriteMessage(const char *fileName) {
    printf("myCp: overwrite '%s'? ", fileName);

    int c = getchar();
    int res = (c == 'y');

    while (c != EOF && c != '\n') {
        c = getchar();
    }

    return res;
}

void copyData(const int fdFrom, const int fdTo) {
    const int bufSize = 4096;
    char buf[bufSize];

    while(1) {
        ssize_t readRes = read(fdFrom, &buf, bufSize);

        if (readRes <= 0) {
            if (readRes < 0) {
                perror("myCp: Read failed");
            }
            break;
        }

        int bufIndex = 0;
        while(1) {
            ssize_t writeRes = write(fdTo, &buf + bufIndex, readRes);

            if (writeRes <= 0) {
                if (readRes < 0) {
                    perror("myCp: Write failed");
                    return;
                }
                break;
            }

            readRes -= writeRes;
            bufIndex += writeRes;
        }

    }

    return;
}

void fileCopy(const char *fileNameFrom, const char *fileNameTo, Parameters_t param) {
    assert(fileNameFrom != NULL && fileNameTo != NULL);

    if (access(fileNameFrom, R_OK) == -1) {
        perror("myCp"); 
        return;
    }

    switch (isFilesSame(fileNameFrom, fileNameTo)) {
        case 1: printf("myCp: '%s' and '%s' are the same file\n", fileNameFrom, fileNameTo); return;
        case -1: perror("myCp"); return;
        default: break;
    }

    int fdFrom = open(fileNameFrom, O_RDONLY);

    if (fdFrom == -1) {
        perror("myCp"); 
        return;
    }

    if (param.iParam && access(fileNameTo, F_OK) != -1 && !overwriteMessage(fileNameTo)) {
        close(fdFrom);
        return;
    }

    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    int fdTo = open(fileNameTo, O_WRONLY | O_TRUNC | O_APPEND | O_CREAT, mode);

    if (fdTo == -1 && param.fParam) {
        if (remove(fileNameTo) == 0) {
            fdTo = open(fileNameTo, O_WRONLY | O_TRUNC | O_APPEND | O_CREAT, mode);
        } else {
            printf("myCp: Cannot remove the file\n");
            close(fdFrom);
            return;
        }
    }
    
    if (fdTo == -1) {
        perror("myCp"); 
        close(fdFrom);
        return;
    }

    copyData(fdFrom, fdTo);

    struct stat st;
    stat(fileNameFrom, &st);
    chmod(fileNameTo, st.st_mode);

    if (param.vParam) {
        printf("'%s' -> '%s'\n", fileNameFrom, fileNameTo);
    }

    close(fdFrom);
    close(fdTo);

    return;
}

int main (int argc, char **argv) {
    Parameters_t param = {0, 0, 0};
    char *fileNames[argc];
    int count = 0;
    int rez = 0;

	while ( (rez = getopt(argc, argv, "-ifv")) != -1) {
		switch (rez) {
            case 'i': param.iParam = 1; break;
            case 'f': param.fParam = 1; break;
            case 'v': param.vParam = 1; break;
            default: fileNames[count] = optarg; count += 1; break; 
		}
	}

    if (count >= 2) {
        if (!isDirExist(fileNames[count - 1])) {
            if (count == 2) {
                fileCopy(fileNames[0], fileNames[1], param);
            } else {
                printf("myCp: Invalid argument input\n");
            }
        } else {
            for (int i = 0; i < count - 1; ++i) {
                if (!isDirExist(fileNames[i])) {
                    char filePath[strlen(fileNames[i]) + strlen(fileNames[count - 1]) + 1];

                    strcat(filePath, fileNames[count - 1]);
                    getNewFilePath(filePath, fileNames[i]);
                    fileCopy(fileNames[i], filePath, param);

                    filePath[0] = '\0';
                } else {
                    printf("myCp: Invalid argument input\n");
                }
            }
        }
    } else {
        printf("myCp: Invalid argument input\n");
    }

    return 0;
}
