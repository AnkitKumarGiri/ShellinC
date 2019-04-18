#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/sem.h>

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#define LSH_RL_BUFSIZE 1024
char *read_line(void)
{
  int bufsize = LSH_RL_BUFSIZE;
  int position = 0;
  char *buffer = malloc(sizeof(char) * bufsize);
  int c;

  if (!buffer) {
    fprintf(stderr, "lsh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  while (1) {
    // Read a character
    c = getchar();

    // If we hit EOF, replace it with a null character and return.
    if (c == EOF || c == '\n') {
      buffer[position] = '\0';
      return buffer;
    } else {
      buffer[position] = c;
    }
    position++;

    // If we have exceeded the buffer, reallocate.
    if (position >= bufsize) {
      bufsize += LSH_RL_BUFSIZE;
      buffer = realloc(buffer, bufsize);
      if (!buffer) {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }
  }
}
#define LSH_TOK_BUFSIZE 64
#define LSH_TOK_DELIM " \t\r\n"
char **parse_command(char *line)
{
  int bufsize = LSH_TOK_BUFSIZE, position = 0;
  char **tokens = malloc(bufsize * sizeof(char*));
  char *token;

  if (!tokens) {
    fprintf(stderr, "lsh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, LSH_TOK_DELIM);
  while (token != NULL) {
    tokens[position] = token;
    position++;

    if (position >= bufsize) {
      bufsize += LSH_TOK_BUFSIZE;
      tokens = realloc(tokens, bufsize * sizeof(char*));
      if (!tokens) {
        fprintf(stderr, "lsh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, LSH_TOK_DELIM);
  }
  tokens[position] = NULL;
  return tokens;
}

void printDir() 
{ 
    char cwd[1024]; 
    getcwd(cwd, sizeof(cwd)); 
    printf("\033[1;35m");
    printf("%s", cwd);
    printf("\033[0m");
} 

void printUser(){
	char *buf;
	buf=(char *)malloc(100*sizeof(char));
	buf=getlogin();
	printf("\033[0;31m");
	printf("%s",buf);
	printf("\033[0m");
}

void prompt(){
	printUser();printf(":");printDir();printf("# ");
}

int isPipe(char *str){
	if(str==NULL)return 0;
	if(strcmp(str,"|")==0)return 1;return 0;
}
#define PIPE 1
#define AND 2
#define OR 3
#define SCOLON 4

int getcode(char *x){
	// printf("%s\n",x);
	if(strcmp("|",x)==0)return PIPE;
	if(strcmp("&&",x)==0)return AND;
	if(strcmp("||",x)==0)return OR;
	if(strcmp(";",x)==0)return SCOLON;
	return 0;
}
void changedir(char *args)
{
  if (args == NULL) {
    fprintf(stderr, "lsh: expected argument to \"cd\"\n");
  } else {
    if (chdir(args) != 0) {
      perror("cd");
    }
  }
}

int execute_command(char **args){
	int n_args=0;
	for(;args[n_args]!=NULL;n_args++);
	if(n_args==0){
		perror("Nothing to execute");return -1;
	}
	int LAST_FLAG=1;
	int status,wpid,pid,inPipeLine=0,code=0,last_code=0,cur_code=0;
	int pd[2];
	int in_fd=0;
	for(int i=0;i<n_args;i++){
		if(strcmp(args[i],"cd")==0){
			changedir(args[i+1]);i++;
			continue;
		}
		if(strcmp(args[i],"exit")==0){
			exit(0);
		}
		cur_code=getcode(args[i]);
		if(cur_code>0){last_code=cur_code;continue;}

		char *argv[10];int k=0,j;
		for(j=i,k=0;j<n_args && (getcode(args[j])==0);j++,k++)argv[k]=args[j];
		argv[k]=NULL;
		i=j-1;
		if(last_code==AND){
			if(!LAST_FLAG){
				LAST_FLAG=1;
				continue;
			}
			pid=fork();
			if(pid==0){
				// char *argv[]={args[i],NULL};
				if (execvp(argv[0], argv) < 0) { 
		            printf("\nCould not execute command"); 
		           	// return -1;
    			} 
    			exit(0);
			}
			else{
				do {
					 wpid = waitpid(pid, &status, WUNTRACED);
			   	} while (!WIFEXITED(status) && !WIFSIGNALED(status));
			   	if(status>0)
			   		LAST_FLAG=0;
			   	
			}
			// printf("andi%d %d\n",i,LAST_FLAG);
			continue;
		}
		if(last_code==OR){
			if(LAST_FLAG){
				continue;
			}
			pid=fork();
			if(pid==0){
				// char *argv[]={args[i],NULL};
				if (execvp(argv[0], argv) < 0) { 
		            printf("\nCould not execute command"); 
		           	// return -1;
    			} 
    			exit(0);
			}
			else{
				do {
					 wpid = waitpid(pid, &status, WUNTRACED);
			   	} while (!WIFEXITED(status) && !WIFSIGNALED(status));
			   	if(status>0)
			   		LAST_FLAG=0;
			}
			// printf("or%d %d\n",i,LAST_FLAG);
			   	continue;
		}
		if(i<n_args-1 && isPipe(args[i+1]))
			inPipeLine=1;
		if(inPipeLine){
			if(isPipe(args[i]))continue;
			if(isPipe(args[i+1])==0)inPipeLine=0;
			pipe(pd);
			pid=fork();
			LAST_FLAG=1;
			if(pid==-1){
				perror("Fork");return -1;
			}
			if(pid==0){
				dup2(in_fd,0);
				if(inPipeLine){//there is a pipeline to follow
					// printf("see %d\n",i);
					dup2(pd[1],1);
				}
				close(pd[0]);
				// char *argv[]={args[i],NULL};
				if (execvp(argv[0], argv) < 0) { 
		            printf("\nCould not execute command"); 
		           	// return -1;
    			}
    			exit(1);
			}
			else{
				do {
					 wpid = waitpid(pid, &status, WUNTRACED);
			   	} while (!WIFEXITED(status) && !WIFSIGNALED(status));
			   	if(status>0)
			   		LAST_FLAG=0;
				close(pd[1]);
				in_fd=pd[0];
				// printf("\nIn parent\n");
			}
			continue;
		}

		LAST_FLAG=1;
		pid=fork();
		if(pid==0){
			// char *argv[]={args[i],NULL};
			if (execvp(argv[0], argv) < 0) { 
	            printf("\nCould not execute command\n"); 
	           	
	           	// return -1;
			} exit(1);
		}
		else{
			do {
				 wpid = waitpid(pid, &status, WUNTRACED);
				
		   	} while (!WIFEXITED(status) && !WIFSIGNALED(status));
		   	if(status>0)
			   		LAST_FLAG=0;
			
		}
		j--;i=j;			
		
	}
}

int main(int argc,char **argv){
	char **args;
	char *comm;
	
	while(1){
		prompt();
		comm= read_line();
		args= parse_command(comm);
		if(execute_command(args)==-1){
			printf("\nError: Could not execute\n");
		}
		free(comm);
		free(args);
	}
	return 0;
}