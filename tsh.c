/* 
 * tsh - A tiny shell program with job control
 * 
 * 20150269 ChangDuHyeok (Dear Professor Cho : This code was writed in Window Platform So Linux Platform may has collapsion)
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */

struct job_t {              /* The job struct */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */


/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv); 
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs); 
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid); 
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid); 
int pid2jid(pid_t pid); 
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
	    break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
	    break;
        case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
	    break;
	default:
            usage();
	}
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1) {

	/* Read command line */
	if (emit_prompt) {
	    printf("%s", prompt);
	    fflush(stdout);
	}
	if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
	    app_error("fgets error");
	if (feof(stdin)) { /* End of file (ctrl-d) */
	    fflush(stdout);
	    exit(0);
	}

	/* Evaluate the command line */
	eval(cmdline);
	fflush(stdout);
	fflush(stdout);
    } 

    exit(0); /* control never reaches here */
}
  
/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
void eval(char *cmdline) 
{
	pid_t pid;									// 프로세스 ID
	sigset_t mask;								// Block할 신호를 정하는데 부가적인 도움을 주는 Signal set 변수
	int background;								// Background Job인지 Foreground Job인지를 구분하는 변수
	char *instruction[MAXARGS], buf[MAXLINE];   // execve()에 들어갈 메개변수들 & 사용자로부터 입력된 Command를 수용할 임시 버퍼

	strcpy(buf, cmdline);						// 임시 버퍼에 사용자로부터 입력된 Command를 임시 저장

	background = parseline(buf, instruction);	// 해당 Command를 Parsing해서 instruction 변수에 저장 후 Back/Foreground 여부를 판별

	if (instruction[0] == NULL) return;         // 빈 Command는 무시함

	if (!builtin_cmd(instruction)) {			// 정의된 Instruction이 아니라면 if 아래 문장을 수행
		sigemptyset(&mask);                     // Signal Set을 어떤 신호를 선택하지 않은 상태로 초기화
		sigaddset(&mask, SIGCHLD);              // Signal Set에 SIGCHLD 신호를 추가함
		sigprocmask(SIG_BLOCK, &mask, NULL);    // Signal Set에 저장된 SIGCHLD 신호를 Block함
		
		if ((pid = fork()) == 0) {				// Child Process일 때
			setpgid(0, 0);							    // Child Process의 Group을 변경
			sigprocmask(SIG_UNBLOCK, &mask, NULL);      // Signal Set에 저장된 SIGCHLD 신호를 Unblock함

			if (execve(instruction[0], instruction, environ) < 0) {	// Child Process에서 해당 Instruction을 실행
				printf("%s: Command not found\n", instruction[0]);
				exit(1);
			}
		}

		// Parent Process일 때
		if (!background) {						// Foreground Job일 때
			addjob(jobs, pid, FG, cmdline);        // Child Process를 Foreground Job으로 Job List에 추가함
			sigprocmask(SIG_UNBLOCK, &mask, NULL); // Signal Set에 저장된 SIGCHLD 신호를 Unblock함
			waitfg(pid);                           // Parent Process는 Foreground Job인 Child Process가 끝나기까지 기다림
		}
		else {									// Background Job일 때
			addjob(jobs, pid, BG, cmdline);                          // Child Process를 Background Job으로 Job List에 추가함
			sigprocmask(SIG_UNBLOCK, &mask, NULL);                   // Signal Set에 저장된 SIGCHLD 신호를 Unblock함
			printf("[%d] (%d) %s", pid2jid(pid), (int)pid, cmdline); // Child Process의 Job ID와 Process ID, Instruction들을 출력함으로써 Background상에서 동작하고 있다는 것을 알림
		}
	}

	return;
}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.  
 */
int parseline(const char *cmdline, char **argv) 
{
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to first space delimiter */
    int argc;                   /* number of args */
    int bg;                     /* background job? */

    strcpy(buf, cmdline);
    buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'') {
	buf++;
	delim = strchr(buf, '\'');
    }
    else {
	delim = strchr(buf, ' ');
    }

    while (delim) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* ignore spaces */
	       buf++;

	if (*buf == '\'') {
	    buf++;
	    delim = strchr(buf, '\'');
	}
	else {
	    delim = strchr(buf, ' ');
	}
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* ignore blank line */
	return 1;

    /* should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0) {
	argv[--argc] = NULL;
    }
    return bg;
}

/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
int builtin_cmd(char **argv) 
{
	if (!strcmp(argv[0], "quit")) {				// quit(종료) 명령어 실행
		exit(0);
	}
	else if (!strcmp(argv[0], "jobs")) {		// jobs(Job List를 출력) 명령어 실행
		listjobs(jobs);
		return 1;
	}
	else if (!strcmp(argv[0], "bg")) {			// bg(Background로 Job을 변경) 명령어 실행
		do_bgfg(argv);
		return 1;
	}
	else if (!strcmp(argv[0], "fg")) {			// fg(Foreground로 Job을 변경) 명령어 실행
		do_bgfg(argv);
		return 1;
	}

	return 0;									// Built-in Command가 아닐 때 0을 반환
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv) 
{
	int jobid;
	struct job_t *j;


	if (argv[1] == NULL) {										// bg, fg 명령어 이후 인자가 NULL이면 예외 처리
		printf("%s command requires PID or %%jobid argument\n", argv[0]);
		return;
	}

	if (!isdigit(argv[1][0]) && argv[1][0] != '%') {            // Process ID나 Job ID가 존재하지 않으면 예외 처리
		printf("%s: argument must be a PID or %%jobid\n", argv[0]);
		return;
	}

	if (argv[1][0] == '%')										// 인자가 Process ID인지 Job ID인지를 구별
		jobid = 1;
	else
		jobid = 0;

	if (jobid) {												// Job ID일 때
		j = getjobjid(jobs, atoi(&argv[1][1]));					// Job ID에 해당하는 job_t 객체 반환

		if (j == NULL) {										// Job ID에 해당하는 job_t 객체가 NULL값이면 예외 처리
			printf("%s: No such job\n", argv[1]);
			return;
		}
	}
	else {														// Process ID일 때
		j = getjobpid(jobs, (pid_t)atoi(argv[1]));				// Process ID에 해당하는 job_t 객체 반환

		if (j == NULL) {										// Job ID에 해당하는 job_t 객체가 NULL값이면 예외 처리
			printf("(%d): No such process\n", atoi(argv[1]));
			return;
		}
	}

	if (strcmp(argv[0], "bg") == 0) {							// Background Job으로 바꾸는 bg 명령어일 때
		j->state = BG;                                          // 해당 job_t 객체의 상태를 Background로 바꿈

		printf("[%d] (%d) %s", j->jid, j->pid, j->cmdline);		// 사용자에게 해당 Job이 Background에서 돌고 있다는 것을 알림
		
		kill(-j->pid, SIGCONT);									// 해당 Job에 SIGCONT Signal을 보내 Continue하게 함
	}
	else {														// Foreground Job으로 바꾸는 bg 명령어일 때
		j->state = FG;                                          // 해당 job_t 객체의 상태를 Foreground로 바꿈

		kill(-j->pid, SIGCONT);									// 해당 Job에 SIGCONT Signal을 보내 Continue하게 함
		waitfg(j->pid);                                         // Foreground Job이 끝날 때까지 기다림
	}

	return;
}

/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
	while (1) {
		if (pid != fgpid(jobs))		// Foreground Job이 끝나면 반복문을 빠져나감으로써 wait의 끝
			break;
		else						// Foreground Job이 진행 상태면 wait
			sleep(1);
	}

	return;
}

/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig) 
{
	pid_t p;
	int s;
	int j;

	while ((p = waitpid(-1, &s, WNOHANG | WUNTRACED)) > 0) {    // Zombie를 Reaping함
		j = pid2jid(p);																		// Process ID에 해당하는 Job ID를 받아옴

		if (WIFEXITED(s))																	// Child Process가 정상적으로 종료되었을 때
			deletejob(jobs, p);																// Child Process를 Job List에서 삭제

		else if (WIFSIGNALED(s)) {															// Child Process가 SIGINT로 인해 종료되었을 때
			deletejob(jobs, p);																// Child Process를 Job List에서 삭제

			printf("Job [%d] (%d) terminated by signal %d\n", j, (int)p, WTERMSIG(s));		// 삭제되었다는 사실을 출력
		}

		else if (WIFSTOPPED(s)) {															// Child Process가 Stop 되었을 때
			getjobpid(jobs, p)->state = ST;													// Job의 상태를 Stop 상태로 설정

			printf("Job [%d] (%d) stopped by signal %d\n", j, (int)p, WSTOPSIG(s));			// Stop되었다는 사실을 출력
		}
	}

	return;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) 
{
	pid_t pid = fgpid(jobs);	// 현재 Foreground Job List를 받아옴

	if (pid != 0)
		kill(-pid, sig);		// 해당 Foreground Job List에 SIGINT Signal을 전달

	return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) 
{
	pid_t pid = fgpid(jobs);	// 현재 Foreground Job List를 받아옴

	if (pid != 0)
		kill(-pid, sig);		// 해당 Foreground Job List에 SIGTSTP Signal을 전달

	return;
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs) 
{
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid > max)
	    max = jobs[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline) 
{
    int i;
    
    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == 0) {
	    jobs[i].pid = pid;
	    jobs[i].state = state;
	    jobs[i].jid = nextjid++;
	    if (nextjid > MAXJOBS)
		nextjid = 1;
	    strcpy(jobs[i].cmdline, cmdline);
  	    if(verbose){
	        printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }
            return 1;
	}
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == pid) {
	    clearjob(&jobs[i]);
	    nextjid = maxjid(jobs)+1;
	    return 1;
	}
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].state == FG)
	    return jobs[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
    int i;

    if (pid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid)
	    return &jobs[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid) 
{
    int i;

    if (jid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid == jid)
	    return &jobs[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid) {
            return jobs[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs) 
{
    int i;
    
    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid != 0) {
	    printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
	    switch (jobs[i].state) {
		case BG: 
		    printf("Running ");
		    break;
		case FG: 
		    printf("Foreground ");
		    break;
		case ST: 
		    printf("Stopped ");
		    break;
	    default:
		    printf("listjobs: Internal error: job[%d].state=%d ", 
			   i, jobs[i].state);
	    }
	    printf("%s", jobs[i].cmdline);
	}
    }
}
/******************************
 * end job list helper routines
 ******************************/


/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler) 
{
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
	unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) 
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}
