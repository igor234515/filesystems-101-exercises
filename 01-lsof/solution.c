#include "solution.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>

static const char *PROC_DIR = "/proc";


void lsof(void)
{
	char file_path[PATH_MAX];
	char lsof_path[2*PATH_MAX];
	char final_path[PATH_MAX];

	DIR *proc_dir = opendir(PROC_DIR);
	if (!proc_dir)
	{
		report_error(PROC_DIR, errno);
		return;
	}
	
	struct dirent *out_directory;

	while ((out_directory = readdir(proc_dir))!= NULL)
	{
		
		char* end_p;
        strtol(out_directory->d_name, &end_p, 10);

        if (*end_p) 
		{
			
            continue;
        }
		
		snprintf(file_path, PATH_MAX, "%s/%s/fd", PROC_DIR, out_directory->d_name);
		//printf("%s\n", file_path);

		if (errno)
		{
			continue;
		}

		DIR *files_dir = opendir(file_path);

		if (!files_dir)
		{
			report_error(file_path, errno);
			continue;
		}
		struct dirent * nested;

		while ((nested = readdir(files_dir))!=NULL)
		{
			//printf("%s\n", nested->d_name );
			char* end_n;
        	strtol(nested->d_name, &end_n, 10);
			
        	if (*end_n) 
			{
				//printf("%c\n", *end_n);
				printf("Not number path\n");
            	continue;
        	}
	
			if (strcmp(out_directory->d_name, ".") == 0 || strcmp(out_directory->d_name, "..") == 0) {
                continue;
            }
			snprintf(lsof_path, 2*PATH_MAX, "%s/%s", file_path, nested->d_name);
			//printf("%s\n", lsof_path);
			if (errno)
			{
				continue;
			}

			ssize_t lsof_lenght;
			
			if ((lsof_lenght = readlink(lsof_path, final_path, PATH_MAX-1)) == -1) 
			{
				//printf("%ld\n", lsof_lenght);
                report_error(lsof_path, errno);
               
			    continue;
            }
			
			final_path[lsof_lenght] = '\0';
            report_file(final_path);
			//printf("%s\n", final_path);
		}
		closedir(files_dir);
	}
	closedir(proc_dir);
}
