// Jason Rosenman

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "shell2.h"

int main(int argc, char** argv) {
	int status, pid, cmd, jobs = 0;
	job* scheduler = malloc(sizeof(job));
	struct timeval before;
	struct rusage usage, old_usage;
	struct timeval after;
	long int cputime, usertime, realtime, old_cputime = 0, old_usertime = 0;
	getrusage(RUSAGE_CHILDREN, &old_usage);
	while(1) {
		char** args = malloc(sizeof(char*)*65);
		cmd = getCommand(args);
		while((pid = waitpid(-1, &status, WNOHANG)) != -1) {
			job* curjob;
			int i;
			for(i = 0; i < jobs; i++) {
				if(scheduler[i].pid == pid) {
					curjob = &scheduler[i];
					break;
				}
			}
			gettimeofday(&(curjob->after), NULL);
			getrusage(RUSAGE_CHILDREN, &usage);
			#ifdef DEBUG
				printf("\nChild %d died with status %d.\n\n", pid, status);
			#endif
			if(WEXITSTATUS(status) != 1) {
				cputime = (usage.ru_stime.tv_sec * 1000) + (usage.ru_stime.tv_usec / 1000);
				usertime = (usage.ru_utime.tv_sec * 1000) + (usage.ru_utime.tv_usec / 1000);
				realtime = ((curjob->after.tv_sec * 1000) + (curjob->after.tv_usec / 1000)) - ((curjob->before.tv_sec * 1000) + (curjob->before.tv_usec / 1000));
				printf("\nProcess statistics for #%d: \n", pid);
				printf("	Elapsed Time: %ld ms\n", realtime);
				printf("	CPU Time: %ld ms\n", cputime - old_cputime);
				printf("	User Time: %ld ms\n", usertime - old_usertime);
				printf("	Involuntary Context Switches: %ld\n", usage.ru_nivcsw - old_usage.ru_nivcsw);
				printf("	Voluntary Context Switches: %ld\n", usage.ru_nvcsw - old_usage.ru_nvcsw);
				printf("	Page Faults: %ld\n", usage.ru_majflt - old_usage.ru_majflt);
				printf("	Reclaimed Page Faults: %ld\n", usage.ru_minflt - old_usage.ru_minflt);
				if(WEXITSTATUS(status) != 0)
					printf("\nProcess exited with status: %d\n", WEXITSTATUS(status));
			}
			old_usage = usage;
			old_cputime = cputime;
			old_usertime = usertime;
			printf("\n");
		}
		if(cmd == CMD_OK) {
			gettimeofday(&before, NULL);
			pid = makeChild(args);
			waitpid(pid, &status, 0);
			if(pid == CMD_ERR)
				exit(1);
			gettimeofday(&after, NULL);
			getrusage(RUSAGE_CHILDREN, &usage);
			free(args);
			#ifdef DEBUG
				printf("\nChild %d died with status %d.\n\n", pid, status);
			#endif
			if(WEXITSTATUS(status) != 1) {
				cputime = (usage.ru_stime.tv_sec * 1000) + (usage.ru_stime.tv_usec / 1000);
				usertime = (usage.ru_utime.tv_sec * 1000) + (usage.ru_utime.tv_usec / 1000);
				realtime = ((after.tv_sec * 1000) + (after.tv_usec / 1000)) - ((before.tv_sec * 1000) + (before.tv_usec / 1000));
				printf("\nProcess statistics for #%d: \n", pid);
				printf("	Elapsed Time: %ld ms\n", realtime);
				printf("	CPU Time: %ld ms\n", cputime - old_cputime);
				printf("	User Time: %ld ms\n", usertime - old_usertime);
				printf("	Involuntary Context Switches: %ld\n", usage.ru_nivcsw - old_usage.ru_nivcsw);
				printf("	Voluntary Context Switches: %ld\n", usage.ru_nvcsw - old_usage.ru_nvcsw);
				printf("	Page Faults: %ld\n", usage.ru_majflt - old_usage.ru_majflt);
				printf("	Reclaimed Page Faults: %ld\n", usage.ru_minflt - old_usage.ru_minflt);
				if(WEXITSTATUS(status) != 0)
					printf("\nProcess exited with status: %d\n", WEXITSTATUS(status));
			}
			old_usage = usage;
			old_cputime = cputime;
			old_usertime = usertime;
			printf("\n");
		}
		else if(cmd == CMD_BG) {
			scheduler[jobs].pid = makeChild(args);
			free(args);
			scheduler[jobs].jobn = jobs + 1;
			printf("[%d] %d\n", scheduler[jobs].jobn, scheduler[jobs].pid);
			gettimeofday(&(scheduler[jobs].before), NULL);
			jobs++;
			scheduler = realloc(scheduler, sizeof(job)*(jobs+1));
		}
		else if(cmd == CMD_EXIT) {
			free(args);
			free(scheduler);
			exit(0);
		}
	}
	return 0;
}

int getCommand(char** args) {
	printf("==] ");
	char* input = malloc(sizeof(char)*130);
	fgets(input, 130, stdin);
	if(feof(stdin)) {
		free(args);
		exit(0);
	}
	#ifdef DEBUG
		printf("Length: %d Last character: %c\n", strlen(input), input[128]);
	#endif
	if(strlen(input) == 129 && input[128] != '\n') {
		fprintf(stderr, "Input string too long.\n");
		while(input[strlen(input)-1] != '\n')
			fgets(input, 130, stdin);
		free(input);
		return CMD_LONG;
	}
	int i = 0;
	char* temp;
	if((temp = strtok(input, " \n")) == NULL) {
		#ifdef DEBUG
			printf("Stray newline detected!\n");
		#endif
		printf("\n");
		args[0] = NULL;
		return CMD_NEWLN;
	}
	else args[0] = strdup(temp);
	#ifdef DEBUG
		printf("Arg 0: %s\n", args[0]);
	#endif
	while((temp = strtok(NULL, " \n")) != NULL) {
		i++;
		#ifdef DEBUG
			printf("Arg %d: %s\n", i, temp);
		#endif
		args[i] = strdup(temp);
	}
	args[i+1] = NULL;
	free(input);
	return checkCmd(args, i);
}

int makeChild(char** args) {
	int cpid = fork();
	if(cpid == 0) {
		#ifdef DEBUG
			printf("Command: %s\n", args[0]);
		#endif
		int error = execvp(args[0], args);
		if(error == -1) {
			if(errno == ENOENT)
				fprintf(stderr, "Command not found.\n");
			else
				fprintf(stderr, "An error occurred while executing the command. Please try again.\n");
			return CMD_ERR;
		}
	}
	return cpid;
}

int checkCmd(char** args, int nargs) {
	if(strcmp(args[0], "exit") == 0) {
		return CMD_EXIT;
	}
	else if(strcmp(args[0], "cd") == 0) {
		if(chdir(args[1]) == -1)
			switch(errno) {
				case EACCES:
					fprintf(stderr, "Permission denied.\n");
					break;
				case ELOOP:
					fprintf(stderr, "Symbolic link loop detected.\n");
					break;
				case ENAMETOOLONG:
					fprintf(stderr, "Path name too long.\n");
					break;
				case ENOENT:
					fprintf(stderr, "Path not found.\n");
					break;
				case ENOTDIR:
					fprintf(stderr, "Not a directory.\n");
					break;
			};
		args[0] = NULL;
		return CMD_BUILTIN;
	}
	else if(strcmp(args[0], "jobs") == 0) {
		int i;
		for(i = 0; i < jobs; i++) {
			//Print scheduled jobs
		}
		return CMD_BUILTIN;
	}
	else if(args[nargs][strlen(args[nargs])-1] == '&') {
		args[nargs] = NULL;
		return CMD_BG;
	}
	return CMD_OK;
}