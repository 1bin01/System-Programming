/* $begin shellmain */
#include "csapp.h"
#include "myshell.h"
#include<errno.h>
#define MAXARGS   128

char PIPE[MAXLINE] = "|";
/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv), H; 
/* cur pid : 현재 foreground에서 돌아가고 있는 process의 pid*/
int cur_pid, Erorr, tmp;
int bg;
sigset_t prev_mask, mask_child, mask_CHLD, prv;

int main() 
{
    char cmdline[MAXLINE]; /* Command line */

    /* install the signal handler*/
    signal(SIGCHLD, sigchld_handler);
    signal(SIGTSTP, sigtstp_handler);
    signal(SIGINT, sigint_handler);

    /* block signal을 통해 race 제거*/
    sigemptyset(&mask_CHLD); sigemptyset(&mask_child);
    sigaddset(&mask_CHLD, SIGCHLD);

    while (1) {
	/* Read */
	printf("CSE4100-SP-P2> ");                   
	fgets(cmdline, MAXLINE, stdin); 
	if (feof(stdin))
	    exit(0);

	/* Evaluate */
	eval(cmdline);
    } 
}
/* $end shellmain */
  
/* $begin eval */
/* eval - Evaluate a commansd line */
void eval(char *cmdline) 
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    strcpy(buf, cmdline);
    bg = parseline(buf, argv);
    if(bg == -1 || H && job_cnt) return; 
    if (argv[0] == NULL)  
	return;   /* Ignore empty lines */

    int pipeidx = -1;
    for(int i = 0; argv[i]; i++)
        if(!strcmp(argv[i], PIPE)){
            pipeidx = i;
            break;
        }

    sigprocmask(SIG_BLOCK, &mask_CHLD, &prev_mask);
    if(pipeidx != -1 || !builtin_command(argv)){
        /* 새로운 child process를 만들어 job에 추가*/
        int pid, status;
        Erorr = 0;
        if(!(pid = Fork())){
            /* 
                child process에 대해서는 SIGCHLD만 받음
                ctrl+z, ctrl+c 에 대해서는 parent process에서 group pid로 처리  
            */
            signal(SIGTSTP, SIG_IGN);
            signal(SIGINT, SIG_IGN);
            sigprocmask(SIG_UNBLOCK, &mask_CHLD, NULL);
            if(!H) setpgid(0, 0);
            go_eval(argv);
            exit(0);
        }
        /* phase2에서는 process가 끝날 때까지 기다림*/
        //if(waitpid(pid, &status, 0) < 0) unix_error("3waitfg: waitpid error\n");

        if(H)
            tmp = waitpid(pid, NULL, 0);
        else{
        //
        char* ix = Jobs[job_cnt + 1];
        /* cmd 명령어를 Jobs에 파싱해서 저장해놓기*/
        for(int i = 0; argv[i]; i++){
            if(i) *(ix++) = ' ';
            strcpy(ix, argv[i]);
            ix += strlen(argv[i]);
        }
        if(!bg) cur_pid = pid;
        else addjob(pid, 1);
        
        /*  cur_pid > 0 foreground 에서 running 중인 process가 있는 경우 */
        if(cur_pid > 0){
            /*
            foreground에서 signal을 받고 넘어가는 경우
            1. process가 끝남 (SIGCHLD)
            2. ctrl+Z 입력 : process가 stopped
            3. ctrl+c 입력 : 
            */    
            // for debugging
            sigsuspend(&prev_mask);
        }
        cur_pid = -1;
        }
    } 
    else if(!H && cur_pid > 0) sigsuspend(&prev_mask);
    /* job을 추가한 후 mask를 복원해서 job을 추가하기 전에 job이 끝나는 상황을 방지*/
    sigprocmask(SIG_SETMASK, &prev_mask, NULL);
    return;
}

void go_eval(char **argv){
    pid_t pid;           /* Process id */
    int pipeidx = -1;   /* pipeline을 가지고 있는지 여부를 체크하고 그 index를 반환 */

    int fd[2], status;
    pid_t pid_left, pid_right;
    char *left_argv[MAXARGS] = {0};
    
    /* pipe의 index 구하기*/
    for(int i = 0; argv[i]; i++)
        if(!strcmp(argv[i], PIPE)){
            pipeidx = i;
            break;
        }

    /* pipe가 존재하는 경우 */
    if(pipeidx != -1){
        if(pipe(fd) < 0) unix_error("pipe error\n");
        for(int i = 0; i < pipeidx; i++) left_argv[i] = argv[i];
        /* left pipe : child process인 경우만 if문을 수행*/
        if(!(pid_left = Fork())){
            close(fd[0]);
            dup2(fd[1], STDOUT_FILENO);
            close(fd[1]);
            go_eval(left_argv);
            exit(0);
        }
        /* pipe의 left child process가 끝날 때까지 기다림*/
        if(waitpid(pid_left, &status, 0) < 0) unix_error("waitfg: waitpid error\n");

        /* right pipe : child process인 경우만 if문을 수행*/
        if(!(pid_right = Fork())){
            close(fd[1]);
            dup2(fd[0], STDIN_FILENO);
            close(fd[0]);
            go_eval(argv + pipeidx + 1);
            exit(0);
        }
        /* pipe의 right child process가 끝날 때까지 기다림*/
        close(fd[0]);
        close(fd[1]);
        if(waitpid(pid_right, &status, 0) < 0) unix_error("waitfg: waitpid error\n");
        return;
    }

    /* pipe가 없는 경우*/
    if (!builtin_command(argv)) { //quit -> exit(0), & -> ignore, other -> run
        if(!(pid = Fork())){
            /*
            execve 는 따로 참조하려는 파일의 PATH를 설정할 수 없음. 따라서 수정이 필요함.
            1. argv[0] (파일 이름) 앞에 경로 /bin/ 까지 추가
            2. execvp를 사용
            */ 
            /*
            이미 /bin에 있는 명령어 : ls, mkdir, rmdir, cat, echo, touch
            */

            if(execvp(argv[0], argv) < 0) { //ex) /bin/ls ls -al &
                printf("%s: Command not found.\n", argv[0]);
                exit(0);
            }
        }
        
	    int status;
        if(waitpid(pid, &status, 0) < 0) unix_error("3waitfg: waitpid error\n");
    }
    return;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv) 
{
    if (!strcmp(argv[0], "quit") || !strcmp(argv[0], "exit")) /* quit command */
	    exit(0);

    if (!strcmp(argv[0], "&"))    /* Ignore singleton & */
	    return 1;
    else if(!strcmp(argv[0], "cd")){
        command_cd(argv);
        return 1;
    }
    else if(!strcmp(argv[0], "jobs")){
        command_jobs(argv);
        return 1;
    }
    else if(!strcmp(argv[0], "bg")){
        command_bg(argv);
        return 1;
    }
    else if(!strcmp(argv[0], "fg")){
        command_fg(argv);
        return 1;
    }
    else if(!strcmp(argv[0], "kill")){
        command_kill(argv);
        return 1;
    }
    return 0;                     /* Not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv) 
{
    char *delim;         /* Points to first space delimiter */
    int argc;            /* Number of args */
    H = 0;
    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
	buf++;
    
    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
        /* 큰 따옴표와 작은 따옴표 처리 */
        if(*buf == '\"' || *buf == '\''){
            char* ed = buf + 1;
            for(;*ed; ed++)
                if(*ed == '\"' || *ed == '\'') break;
            if(*ed && (*buf == *ed)){
                argv[argc++] = buf + 1;
                *ed = '\0';
                buf = ed + 1;
            }
            else {
                /* error */ 
                printf("invalid syntax\n");
                return -1;
            }
        }
        else if(*buf == '|'){
            /* Pipeline 처리 */
            argv[argc++] = PIPE;
            buf = buf + 1;
        }
        else{
            char *pipePtr = NULL;
            pipePtr = strchr(buf, '|');
	        argv[argc++] = buf;
            /* pipe line이 명령어와 붙어있는 경우 예외 처리*/
            if(pipePtr && pipePtr < delim){
                *pipePtr = '\0';
                argv[argc++] = PIPE;
                buf = pipePtr + 1;
            }
            else{
	            *delim = '\0';
	            buf = delim + 1;
            }
        }
	    while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    }
    argv[argc] = NULL;
    if (argc == 0)  /* Ignore blank line */
	return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0)
	    argv[--argc] = NULL;
    else{
        /* &가 명령어와 붙어있는 경우 예외처리 */
        int e = strlen(argv[argc - 1]) - 1;
        bg = argv[argc - 1][e] == '&';
        if(bg) argv[argc - 1][e] = '\0'; 
    }
    for(int i = 0; i < argc; i++) if(!strcmp(argv[i], "less")) H = 1;
    return bg;
}
/* $end parseline */

void command_cd(char* argv[]){
    /* <unistd.h> 헤더에 있는 chdir 명령어를 이용*/
    if(argv[2] != NULL) {
        /* error : 인자를 너무 많이 준 경우 */
        printf("bash: cd: too many arguments\n");
    }
    /* 인자를 주지 않은 경우 HOME directory로 이동 */
    else if(argv[1] == NULL) {
        /* error : HOME directory가 정의되어 있지 않은 경우*/
        if(chdir(getenv("HOME")) == -1) printf("bash: cd: home directory is undefined\n");
    }
    else if(chdir(argv[1]) == -1) 
        /* error : 입력으로 받은 이름을 가진 file이나 directory가 없는 경우*/
        printf("bash: cd: %s: No such file or directory\n", argv[1]);
    return;
}  

void command_jobs(char* argv[]){
    for(int i = 1; i <= job_cnt; i++)
        if(job_state[i] == 1) printf("[%d] running %s\n", i, Jobs[i]);
        else if(job_state[i] == -1) printf("[%d] suspended %s\n", i, Jobs[i]);
    return;
}

void command_bg(char* argv[]){
    if(argv[1] && argv[1][0] == '%'){
        int job_num = atoi(argv[1] + 1);
        /* 현재 job_num을 가진 job이 존재하는 경우*/
        if(job_num >= 1 && job_num <= job_cnt && job_state[job_num]){
            if(job_state[job_num] == 1) printf("job %d already in background\n", job_num);
            else{
                job_state[job_num] = 1;
                printf("[%d] running %s\n", job_num, Jobs[job_num]);
                kill(jpid[job_num], SIGCONT);
            }
            return;
        }
    }
    printf("No Such Job\n");
    return;
}

void command_fg(char* argv[]){
    if(argv[1] && argv[1][0] == '%'){
        int job_num = atoi(argv[1] + 1);
        /* 현재 job_num을 가진 job이 존재하는 경우*/
        if(job_num >= 1 && job_num <= job_cnt && job_state[job_num]){
            job_state[job_num] = 1;
            printf("[%d] running %s\n", job_num, Jobs[job_num]);
            kill(jpid[job_num], SIGCONT);
            cur_pid = jpid[job_num];
            return;
        }
    }
    printf("No Such Job\n");
    return;
}

void command_kill(char* argv[]){
    if(argv[1] && argv[1][0] == '%'){
        int job_num = atoi(argv[1] + 1);
        /* 현재 job_num을 가진 job이 존재하는 경우*/
        if(job_num >= 1 && job_num <= job_cnt && job_state[job_num]){
            job_state[job_num] = 0;
            kill(jpid[job_num], SIGINT);
            return;
        }
    }
    printf("No Such Job\n");
    return;
}

void addjob(int pid, int st){
    jpid[++job_cnt] = pid;
    job_state[job_cnt] = st;
    return;
}

/* child process가 종료되었을 때 처리하는 signal handler*/
void sigchld_handler(int s){
    int olderrno = errno;
    int pid;
    while((pid = wait(NULL)) > 0){
        /* running 중이던 process가 끝나면 terminated 상태로 update*/
        for(int i = 1; i <= job_cnt; i++)
            if(jpid[i] == pid){
                job_state[i] = 0;
                return;
            }
        /* foreground 작업이 종료된 경우 */
        if(pid == cur_pid) {
            cur_pid = -1;
            return;
        }
    }
    errno = olderrno;
    return;
}


/* ctrl+z를 입력했을 때 처리하는 signal handler*/
void sigtstp_handler(int s){

    int olderrno = errno;
    /* -pid를 인자로 준다 : process group에 속하는 모든 process를 kill*/
    if(cur_pid > 0 && !kill(-cur_pid, SIGTSTP)){
        int injob = 0;
        for(int i = 1; i <= job_cnt; i++)
            if(jpid[i] == cur_pid){
                injob = 1;
                job_state[i] = -1;
            }
        if(!injob) addjob(cur_pid, -1);
        // printf("\n[%d] suspended %s\n", job_cnt, Jobs[job_cnt]);
        printf("\n");
        cur_pid = -1;
    }
    errno = olderrno;
    return;
}

/* ctrl+c를 입력했을 때 처리하는 signal handler*/
void sigint_handler(int s){

    int olderrno = errno;
    /* -pid를 인자로 준다 : process group에 속하는 모든 process를 kill*/
    if(cur_pid > 0){
        kill(-cur_pid, SIGINT);
        for(int i = 1; i <= job_cnt; i++)
            if(jpid[i] == cur_pid) {
                job_state[i] = 0;
                break;
            }
        printf("\n");
    }
    errno = olderrno;
    cur_pid = -1;
    return;
}