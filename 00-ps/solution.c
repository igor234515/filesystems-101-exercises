#include "solution.h"
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define envSIZE 128
struct processInfo
{
    char *exe;
    char **argv;
    char **envp;
    pid_t pid;
    size_t argv_length;
    size_t envp_length;
};
static int readCmdLine(struct processInfo *processInfo);
static int readEnviron(struct processInfo *processInfo);
static int readExe(struct processInfo *processInfo);
static int getProcessInfo(struct processInfo *processInfo);
static int cleanProcessInfo(struct processInfo *processInfo);
static int isProcess(struct dirent *_proc_entry);
void ps(void)
{
    /* implement me */

    DIR *proc;

    struct dirent *proc_entr;

    proc = opendir("/proc");

    if (!proc)
    {
        report_error("/proc", errno);
        exit(1);
    };

    struct processInfo *processInfo = (struct processInfo *)calloc(1, sizeof(struct processInfo));

    while ((proc_entr = readdir(proc)) != NULL)
    {

        if (!(processInfo->pid = isProcess(proc_entr)))
        {
            continue;
        }

        int err = getProcessInfo(processInfo);
        if (!err)
        {
            report_process(processInfo->pid, processInfo->exe, processInfo->argv, processInfo->envp);
        }
        cleanProcessInfo(processInfo);
    };

    free(processInfo);
    closedir(proc);
}

int readEnviron(struct processInfo *processInfo)
{
    char pathName[PATH_MAX];
    sprintf(pathName, "/proc/%d/environ", processInfo->pid);

    FILE *fd = fopen(pathName, "r");

    processInfo->envp_length = envSIZE;
    processInfo->envp = (char **)calloc(processInfo->envp_length, sizeof(char *));

    if (fd)
    {
        size_t len = 0;
        for (size_t i = 0; i < processInfo->envp_length; ++i)
        {
            if (i == processInfo->envp_length - 1)
            {
                processInfo->envp_length *= 2;
                processInfo->envp = realloc(processInfo->envp, processInfo->envp_length * sizeof(char *));
            }
            processInfo->envp[i] = NULL;

            if (getdelim(&processInfo->envp[i], &len, '\0', fd) == -1)
            {
                free(processInfo->envp[i]);
                processInfo->envp[i] = NULL;
                break;
            }
            if (processInfo->envp[i][0] == '\0')
            {
                free(processInfo->envp[i]);
                processInfo->envp[i] = NULL;
                break;
            }
        }
        fclose(fd);
    }
    else
    {
        report_error(pathName, errno);
        return 1;
    }
    return 0;
}

int readCmdLine(struct processInfo *processInfo)
{
    char pathName[PATH_MAX];
    sprintf(pathName, "/proc/%d/cmdline", processInfo->pid);

    FILE *fd = fopen(pathName, "r");

    processInfo->argv_length = envSIZE;
    processInfo->argv = (char **)calloc(processInfo->argv_length, sizeof(char *));

    if (fd)
    {
        size_t len = 0;
        for (size_t i = 0; i < processInfo->argv_length; ++i)
        {
            if (i == processInfo->argv_length - 1)
            {
                processInfo->argv_length *= 2;
                processInfo->argv = realloc(processInfo->argv, processInfo->argv_length * sizeof(char *));
            }

            processInfo->argv[i] = NULL;

            if ((getdelim(&processInfo->argv[i], &len, '\0', fd)) == -1)
            {
                free(processInfo->argv[i]);
                processInfo->argv[i] = NULL;
                break;
            }
            if (processInfo->argv[i][0] == '\0')
            {
                free(processInfo->argv[i]);
                processInfo->argv[i] = NULL;
                break;
            }
        }

        fclose(fd);
    }

    else
    {
        report_error(pathName, errno);
        return 1;
    }
    return 0;
}

int readExe(struct processInfo *processInfo)
{
    char pathName[PATH_MAX];

    sprintf(pathName, "/proc/%d/exe", processInfo->pid);

    processInfo->exe = calloc(PATH_MAX, sizeof(char));

    return (readlink(pathName, processInfo->exe, PATH_MAX) == -1);
}
int getProcessInfo(struct processInfo *processInfo)
{
    int error;

    error = readCmdLine(processInfo);

    error |= readEnviron(processInfo);

    error |= readExe(processInfo);

    if (error)
    {
        return error;
    }
    return 0;
}
int cleanProcessInfo(struct processInfo *processInfo)
{
    for (size_t i = 0; i < processInfo->argv_length; ++i)
    {
        if (processInfo->argv[i] == NULL)
        {
            break;
        }
        free(processInfo->argv[i]);
    }
    for (size_t i = 0; i < processInfo->envp_length; ++i)
    {
        if (processInfo->envp[i] == NULL)
        {
            break;
        }
        free(processInfo->envp[i]);
    }

    if (processInfo->exe)
    {
        free(processInfo->exe);
    }

    if (processInfo->argv)
    {
        free(processInfo->argv);
    }

    if (processInfo->envp)
    {
        free(processInfo->envp);
    }

    return 0;
}
int isProcess(struct dirent *_proc_entry)
{
    if (_proc_entry->d_type != DT_DIR)
    {
        return 0;
    }

    return atoi(_proc_entry->d_name);
}
