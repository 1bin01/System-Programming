/* $begin shellmain */
#include "csapp.h"
#include "myshell.h"
#include<errno.h>
#define MAXARGS   128

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
/* eval - Evaluate a command line */
void eval(char *cmdline) 
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */
    
    strcpy(buf, cmdline);
    bg = parseline(buf, argv); 
    if (argv[0] == NULL)  
	return;   /* Ignore empty lines */
    if (!builtin_command(argv)) { //quit -> exit(0), & -> ignore, other -> run
        /*child process인 경우만 if문을 수행*/
        if((!(pid = Fork()))){
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
            /* back ground에서 실행하는 경우가 아니면 child process가 terminated될 때까지 기다린다.*/ 
            if(waitpid(pid, &status, 0) < 0) unix_error("waitfg: waitpid error\n");
	    }
	    else//when there is backgrount process!
	        printf("%d %s", pid, cmdline);
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
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
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