#pragma once

#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
/**
   Implement this function to list processes. It must call report_process()
   for each running process. If an error occurs when accessing a file or
   a directory, it must call report_error().
*/
void ps(void);

/**
   ps() must call this function to report each running process.
   @exe is the absolute path to the executable file of the process
   @argc is a NULL-terminated array of command line arguments to the process
   @envp is a NULL-terminated array of environment variables of the process
*/
void report_process(pid_t pid, const char *exe, char **argv, char **envp);
/**
   ps() must call this function whenever it detects an error when accessing
   a file or a directory.
*/
void report_error(const char *path, int errno_code);

//additional
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

int readCmdLine(struct processInfo *processInfo);
int readEnviron(struct processInfo *processInfo);
int readExe(struct processInfo *processInfo);
int getProcessInfo(struct processInfo *processInfo);
int cleanProcessInfo(struct processInfo *processInfo);
int isProcess(struct dirent *proc_entry);