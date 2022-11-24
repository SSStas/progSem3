#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <time.h>
#include <string.h>
#include <errno.h>
#include <malloc.h>
#include <assert.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>


enum PARAM_NAMES { 
    L_ARG = 0,
    D_ARG = 1,
    A_ARG = 2,
    R_ARG = 3,
    I_ARG = 4,
    N_ARG = 5
};

typedef struct Parameters_t {
    int args;
    char path[PATH_MAX];
} Parameters_t;

int isArg(Parameters_t *param, enum PARAM_NAMES name) {
    return param->args & (1 << name);
}

int isDirExist(const char *folder) {
    assert(folder != NULL);

    struct stat sb;
    return (stat(folder, &sb) == 0 && S_ISDIR(sb.st_mode));
}

char *addFileInPath(char *folder, const char *fileName) {
    assert(folder != NULL && fileName != NULL);
    
    if (folder[strlen(folder) - 1] != '/' && folder[0] != '\0') {
        return strcat(strcat(folder, "/"), fileName);
    }

    return strcat(folder, fileName);
}

void popLastFileFromPath(char *folder) {
    assert(folder != NULL);

    for (int i = strlen(folder) - 2; i >= 0; --i) {
        if (folder[i] == '/') {
            folder[i + 1] = '\0';
            return;
        }
    }
}

void printFileMode(mode_t mode, char *term) {
    char modeText[10];

    modeText[0] = (S_ISDIR(mode) ? 'd' : (S_ISLNK(mode) ? 'l' : '-'));
    modeText[1] = ((mode & S_IRUSR) ? 'r' : '-');
    modeText[2] = ((mode & S_IWUSR) ? 'w' : '-');
    modeText[3] = ((mode & S_IXUSR) ? 'x' : '-');
    modeText[4] = ((mode & S_IRGRP) ? 'r' : '-');
    modeText[5] = ((mode & S_IWGRP) ? 'w' : '-');
    modeText[6] = ((mode & S_IXGRP) ? 'x' : '-');
    modeText[7] = ((mode & S_IROTH) ? 'r' : '-');
    modeText[8] = ((mode & S_IWOTH) ? 'w' : '-');
    modeText[9] = ((mode & S_IXOTH) ? 'x' : '-');

    printf("%s%s", modeText, term);
}

void printLink(char *path, struct stat st, char *term) {
    char *buf;
    ssize_t bufSize = st.st_size + 1, countBytes = 0;

    if (!S_ISLNK(st.st_mode)) {
        return;
    }

    if (st.st_size == 0) {
        bufSize = PATH_MAX;
    }

    buf = malloc(bufSize);
    if (buf == NULL) {
        perror("myLs");
        return;
    }

    countBytes = readlink(path, buf, bufSize);
    if (countBytes == -1) {
        perror("myLs");
        free(buf);
        return;
    }

    buf[countBytes] = '\0';

    printf("-> %s%s", buf, term);

    free(buf);
}

void printUserGroupNames(Parameters_t *param, uid_t userId, gid_t groupId, char *term) {
    if (isArg(param, N_ARG)) {
        printf("%4u %4u%s", userId, groupId, term);
    } else if (isArg(param, L_ARG)) {
        struct passwd *u = getpwuid(userId);
        struct group *g = getgrgid(groupId);

        printf("%10s %10s%s", u->pw_name, g->gr_name, term);
    }
}

void printModTime(struct stat st, char *term) {
    char *mon[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    time_t realTime = time(NULL);
    struct tm *tmReal = localtime(&realTime);
    int year = tmReal->tm_year;

    time_t time = st.st_mtime;
    struct tm *tmNow = localtime(&time);
    
    printf("%s %2d ", mon[tmNow->tm_mon], tmNow->tm_mday);

    if (year == tmNow->tm_year) {
        printf("%02d:%02d%s", tmNow->tm_hour, tmNow->tm_min, term);
    } else {
        printf("%5d%s", 1900 + tmNow->tm_year, term);
    }

}

void showFileStat(Parameters_t *param, const char *fileName) {
    if (fileName != NULL && !isArg(param, A_ARG) && fileName[0] == '.') {
        return;
    }

    if (fileName != NULL) {
        addFileInPath(param->path, fileName);
    }

    struct stat st;
    if (lstat(param->path, &st) == -1) {
        perror("myLs");
        popLastFileFromPath(param->path);
        return;
    }

    if(isArg(param, I_ARG)) {
        printf("%-20ld ", st.st_ino);
    }

    if (isArg(param, L_ARG) || isArg(param, N_ARG)) {
        printFileMode(st.st_mode, " ");
        printf("%3lu ", st.st_nlink);
        printUserGroupNames(param, st.st_uid, st.st_gid, " ");
        printf("%7lu ", st.st_size);
        printModTime(st, " ");
        printf("%s ", (fileName != NULL) ? fileName : param->path);
        printLink(param->path, st, "");
    } else {
        printf("%s", (fileName != NULL) ? fileName : param->path);
    }

    printf("\n");

    if (fileName != NULL) {
        popLastFileFromPath(param->path);
    }

    return;
}

void printTotalBlocksCount(Parameters_t *param, char *term) {
    struct stat st;
    size_t count = 0;
    DIR *d = opendir(param->path);
    struct dirent *e = NULL;

    while((e = readdir(d)) != NULL) {
        if (!isArg(param, A_ARG) && e->d_name[0] == '.') {
            continue;
        }

        addFileInPath(param->path, e->d_name);

        if (lstat(param->path, &st) == -1) {
            perror("myLs");
            popLastFileFromPath(param->path);
            continue;
        }

        count += st.st_blocks;
        
        popLastFileFromPath(param->path);
    } 

    closedir(d);

    printf("total %lu%s", count / 2, term);
}

void showFilesData(Parameters_t *param, int isShowFolderName, int isShowFilesContents) {
    if (isArg(param, D_ARG)) {
        if (access(param->path, F_OK) == -1) {
            perror("myLs");
            return;
        }
        showFileStat(param, NULL);
        return;
    }

    if (!isDirExist(param->path)) {
        return;
    }

    DIR *d = opendir(param->path);
    struct dirent *e = NULL;

    if (isShowFolderName) {
        printf("%s:\n", param->path);
    }

    if (!isShowFilesContents && (isArg(param, L_ARG) || isArg(param, N_ARG))) {
        printTotalBlocksCount(param, "\n");
    }

    while((e = readdir(d)) != NULL) {
        if (isShowFilesContents) {
            if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) {
                if (!isArg(param, A_ARG) && e->d_name[0] == '.') {
                    continue;
                }
                addFileInPath(param->path, e->d_name);
                showFilesData(param, 1, 0);
                popLastFileFromPath(param->path);
            }
        } else {
            showFileStat(param, e->d_name);
        }
    }  

    if (!isShowFilesContents) {
        printf("\n");
    }

    if (errno != 0) {
        perror("myLs");
    }

    closedir(d);

    if (isArg(param, R_ARG) && !isShowFilesContents) {
        showFilesData(param, 0, 1);
    }
}

int main (int argc, char **argv) {
    Parameters_t param = {0, "."};
    char *folder[argc];
    int count = 0;
    int rez = 0;

	while ( (rez = getopt(argc, argv, "-ldaRin")) != -1) {
		switch (rez) {
            case 'l': param.args |= (1 << L_ARG); break;
            case 'd': param.args |= (1 << D_ARG); break;
            case 'a': param.args |= (1 << A_ARG); break;
            case 'R': param.args |= (1 << R_ARG); break;
            case 'i': param.args |= (1 << I_ARG); break;
            case 'n': param.args |= (1 << N_ARG); break;
            default: folder[count] = optarg; count += 1; break; 
		}
	}

    if (count == 0) {
        showFilesData(&param, isArg(&param, R_ARG) ? 1 : 0, 0);
    } else {
        for (int i = 0; i < count; ++i) {
            param.path[0] = '\0';
            addFileInPath(param.path, folder[i]);
            showFilesData(&param, 1, 0);
        }
    }

    return 0;
}
