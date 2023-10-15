#include "solution.h"

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
