/* $begin shellmain */
#include "csapp.h"
#include "myshell.h"
#include<errno.h>
#define MAXARGS   128

char PIPE[MAXLINE] = "|";
/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv); 

int main() 
{
    char cmdline[MAXLINE]; /* Command line */

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
    int bg;              /* Should the job run in bg or fg? */
    strcpy(buf, cmdline);
    bg = parseline(buf, argv); 
    if (bg == -1 || argv[0] == NULL)  
	return;   /* Ignore empty lines */

    int pipeidx = -1;
    for(int i = 0; argv[i]; i++)
        if(!strcmp(argv[i], PIPE)){
            pipeidx = i;
            break;
        }
    if(pipeidx != -1 || !builtin_command(argv)){
        /* 새로운 child process를 만들어 job에 추가*/
        int pid, pgid, status;
        if(!(pid = Fork())){
            go_eval(argv);
            exit(0);
        }
        /* phase2에서는 process가 끝날 때까지 기다림*/
        if(waitpid(pid, &status, 0) < 0) unix_error("3waitfg: waitpid error\n");
        

        /* phase3
            signal bock하기
            addjob(pid)
        */
    }
    return;
}

void go_eval(char **argv){
    int bg = 0;              /* Should the job run in bg or fg? */
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
        
	    /* Parent waits for foreground job to terminate */
	    if (!bg){ 
	        int status;
            if(waitpid(pid, &status, 0) < 0) unix_error("3waitfg: waitpid error\n");
	    }
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
    return 0;                     /* Not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv) 
{
    char *delim;         /* Points to first space delimiter */
    int argc;            /* Number of args */
    int bg;              /* Background job? */

    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
	buf++;
    
    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
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
                printf("invalid syntax\n");
                return -1;
            }
        }
        else if(*buf == '|'){
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

    return bg;
}
/* $end parseline */

void command_cd(char* argv[]){
    /* <unistd.h> 헤더에 있는 chdir 명령어를 이용*/
    /* 인자를 주지 않은 경우 HOME directory로 이동 */
    if(argv[1] == NULL) {
        /* error : HOME directory가 정의되어 있지 않은 경우*/
        if(chdir(getenv("HOME")) == -1) printf("bash: cd: home directory is undefined\n");
    }
    else if(chdir(argv[1]) == -1) 
        /* error : 입력으로 받은 이름을 가진 file이나 directory가 없는 경우*/
        printf("bash: cd: %s: No such file or directory\n", argv[1]);
    return;
}  