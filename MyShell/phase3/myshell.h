#include <stdio.h>

#define MAXJOBS 205

void command_cd(char* argv[]);
void go_eval(char* argv[]);
void command_jobs(char* argv[]);
void command_bg(char* argv[]);
void command_fg(char* argv[]);
void command_kill(char* argv[]);
void addjob(int pid, int);

char Jobs[MAXJOBS][MAXLINE];
int job_cnt, jpid[MAXJOBS], idx = 1000;
/* 0 : terminated, 1 : running, -1 : suspended*/
int job_state[MAXJOBS];

/* signal handler */
void sigchld_handler(int);
void sigtstp_handler(int);
void sigint_handler(int);