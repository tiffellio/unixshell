/***********************************************************************************************************************
 * Simple UNIX Shell
 * @author: Tiffany Elliott (csci username: elliottt, student ID: 577566797)
 * CSCI360, FALL 2019 
 ***********************************************************************************************************************/
#include <string.h>
#include<stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include<fcntl.h> 
#include<errno.h> 
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_LENGTH 80 // The maximum length for commands

//If command contains output redirection argument
//  fork a child process invoking fork() system call and perform the followings in the child process:
//      open the redirected file in write only mode invoking open() system call
//      copy the opened file descriptor to standard output file descriptor (STDOUT_FILENO) invoking dup2() system call
//      close the opened file descriptor invoking close() system call
//      change the process image with the new process image according to the UNIX command using execvp() system call
//  If command does not conatain & (ampersand) at the end
//      invoke wait() system call in parent processs
void outputRedirection(char *arr[], int cmd);

//If command contains input redirection argument
//  fork a child process invoking fork() system call and perform the followings in the child process:
//      open the redirected file in read  only mode invoking open() system call
//      copy the opened file descriptor to standard input file descriptor (STDIN_FILENO) invoking dup2() system call
//      close the opened file descriptor invoking close() system call
//      change the process image with the new process image according to the UNIX command using execvp() system call
//  If command does not conatain & (ampersand) at the end
//      invoke wait() system call in parent process
void inputRedirection(char *arr[], int cmd);

//If command contains pipe argument
//  fork a child process invoking fork() system call and perform the followings in the child process:
//      create a pipe invoking pipe() system call
//      fork another child process invoking fork() system call and perform the followings in this child process:
//          close the write end descriptor of the pipe invoking close() system call
//          copy the read end  descriptor of the pipe to standard input file descriptor (STDIN_FILENO) invoking dup2() system call
//          change the process image of the this child with the new image according to the second UNIX command after the pipe symbol (|) using execvp() system call
//      close the read end descriptor of the pipe invoking close() system call
//      copy the write end descriptor of the pipe to standard output file descriptor (STDOUT_FILENO) invoking dup2() system call
//      change the process image with the new process image according to the first UNIX command before the pipe symbol (|) using execvp() system call
//If command does not conatain & (ampersand) at the end invoke wait() system call in parent process
void pipeArgument(char *arr[], int len, int mrk, bool ampFlag);

//Helper function that will clear the contents of the array
void resetArray(char *arr[], int len);

int main(void) {

    char command[MAX_LENGTH]; //Holds the users initial commands, unparsed
    char *args[MAX_LENGTH/2 + 1]; // Maximum 40 arguments

    //Token arrays, holding the contents of key commands 
    //  we will use these to set the appropriate flags 
    char outToken[2] = ">";
    char inToken[2]="<";
    char pipeToken[2]="|";
    char exitToken[5]="exit";
    char ampToken[2]="&";
 
    //Flags for us to call the appropriate function based on users command
    bool hasAmp, hasOutput, hasInput, hasPipe, isExit = false;

    int should_run = 1; //used to control our free-running loop

    int count = 0; //word/token index count
    int ampIndex = 0; //holds ampersand index
    int tokenPos = 0;//holds the location of a special token such as >,<, or |

    while (should_run) {
       
        //Prompt the user
        printf("ssh>>"); 
        fflush(stdout);
        fgets(command, MAX_LENGTH, stdin);
        
        //Parse each argument via spaces,
        //(strtok ends tokens with \n so we will add these to our delimiter list as well)
        const char delim[3] = " \n"; 
        
        //Set the first token from the command 
        char *token; 
        token = strtok(command, delim);

        //Parse command and arguments   
        while( token != NULL ) {

            args[count] = token; //Fill the arguement array with tokens

            //If the arguement matches a special character token
            //  set the flag and mark the token's position 
            if (strcmp(token,outToken) == 0){
                hasOutput = true;
                tokenPos = count;
            } else if (strcmp(token,inToken) == 0){
                hasInput = true;
                tokenPos = count;
            } else if (strcmp(token,pipeToken) == 0){
                hasPipe = true;
                tokenPos = count;
            }

            //Otherwise, if the user enters the "exit token", we will end our program
            else if (((strcmp(token,exitToken)== 0) || (strcmp(token,exitToken)== 10)) && (count == 0)){
                printf("Closing Simple Shell, goodbye!\n");
                should_run = 0;
                isExit = true; //ensure shell will not execute token after loop
            }

            count++; 

            token = strtok(NULL,delim);//Step through to the next token, ignoring the delimiters
        }

        //if user entered CRLF, prompt user for input again
        if(count == 0) continue;

        //If the last parsed  argument in the users command is an ampersand & 
        //  set the appropriate flag (so we can handle as a background process)
        //      and discard the symbol from our array.
        ampIndex = (count-1); // resize array
        if(strcmp(args[count-1], ampToken) == 0){
            hasAmp = true;
            args[ampIndex] = NULL;                            
        } 
        
        /***Call appropriate functions for commands: if the flag was raised
          otherwise execute as a regular command***/
        //Fork a child process and call outputRedirection() to handle the command
        if(hasOutput){
            pid_t pid = fork(); //fork a child process
            if(pid == 0){
                hasOutput = false; //Clear condition for next command
                outputRedirection(args, tokenPos);
            } else {
                    if(hasAmp == false){ //If the user did set as &, the parent will wait
                        waitpid(pid, NULL, 0);
                        resetArray(args, count);
                    } else {
                        hasAmp = false; //Reset the flag
                    }
            }

        //Fork a child process and call inputRedirection() to handle the command
        } else if(hasInput){
            pid_t pid = fork();
            if(pid == 0){
                hasInput = false; //clear condition for next command
                inputRedirection(args, tokenPos);
            } else {
                    if(hasAmp == false){ //If the user did set as &, the parent will wait
                        waitpid(pid, NULL, 0);
                        resetArray(args, count);
                    } else {
                        hasAmp = false; //reset the flag
                    }
            }

        //Call pipeArgument() to handle pipe command, passing in the size, position of pipe
        //  and flag for &
        } else if(hasPipe){
            hasPipe = false; //clear condition for next command
            pipeArgument(args, count, tokenPos, hasAmp);
            resetArray(args, count);

        //If command does not contain any of the above
        //  fork a child process using fork() system call and perform the followings in the child process.
        //  change the process image with the new process image according to the UNIX command using execvp() system call
        //  If command does not conatain & (ampersand) at the end
        //  invoke wait() system call in parent process
        } else {
            if(isExit == false){ //ensure that exit command flag has not been raised so we do not execute this command
                pid_t pid = fork();
                if(pid == 0){
                    if (execvp(args[0],args) == -1){
                        printf("Regular Command failed.\n");
                        resetArray(args, count);
                        exit(0);
                    }
                } else {
                    if(hasAmp == false){ //If the user did set as &, the parent will wait
                        waitpid(pid,NULL,0);
                    } else {
                        hasAmp = false; //reset the flag
                        
                    }
                }
            }
        }

        //Reset arguement array, flags, and position markers before next command
        resetArray(args, count);
        hasInput = false;
        hasOutput = false;
        hasPipe = false;
        count = 0;
        ampIndex = 0;
        tokenPos = 0;
    }  
    return 0; 
}

//If command contains output redirection argument
//  fork a child process invoking fork() system call and perform the followings in the child process:
//      open the redirected file in write only mode invoking open() system call
//      copy the opened file descriptor to standard output file descriptor (STDOUT_FILENO) invoking dup2() system call
//      close the opened file descriptor invoking close() system call
//      change the process image with the new process image according to the UNIX command using execvp() system call
//  If command does not conatain & (ampersand) at the end
//      invoke wait() system call in parent processs
void outputRedirection(char *arr[], int cmd){

    //Copy over arguements before > command
    char *pre[cmd];
    for(int i = 0; i < cmd; i++){
        pre[i] = arr[i];
    }
    pre[cmd]=NULL; // Overwrite "<" symbol

    //Open file (or create if it doesn't exist) that we want to read from and use
    //   STDOUT to handle output redirection, then close the file    
    int fd = open(arr[cmd+1], O_WRONLY | O_CREAT, 0666);
    dup2(fd, STDOUT_FILENO);
    close(fd);


    //Execution command, if it failed, generate an error
    if (execvp(pre[0],pre) == -1){
        printf("Sorry, the output redirection command failed to execute. \n");
    } else if(execvp(arr[cmd], arr) == -1){
        printf("Sorry, the output redirection command failed to execute.  \n");
    }
}

//If command contains input redirection argument
//  fork a child process invoking fork() system call and perform the followings in the child process:
//      open the redirected file in read  only mode invoking open() system call
//      copy the opened file descriptor to standard input file descriptor (STDIN_FILENO) invoking dup2() system call
//      close the opened file descriptor invoking close() system call
//      change the process image with the new process image according to the UNIX command using execvp() system call
//  If command does not conatain & (ampersand) at the end
//      invoke wait() system call in parent process
void inputRedirection(char *arr[], int cmd){

    //Copy over arguements before < command
    char *pre[cmd];
    for(int i = 0; i < cmd; i++){
        pre[i] = arr[i];
    }
    pre[cmd]=NULL; // Overwrite "<" symbol

    //Open the file we want to read from and out STDIN to handle
    //  input redirection, then close the file
    int fd = open(arr[cmd+1], O_RDONLY, 0666);
    dup2(fd, STDIN_FILENO); 
    close(fd);

    //Execution command, if it failed, generate an error
    if (execvp(pre[0],pre) == -1){
        printf("Sorry, the input redirection command failed to execute. \n");
    } else if(execvp(arr[cmd], arr) == -1){
        printf("Sorry, the input redirection command failed to execute. \n");
    }    
}

//If command contains pipe argument
//  fork a child process invoking fork() system call and perform the followings in the child process:
//      create a pipe invoking pipe() system call
//      fork another child process invoking fork() system call and perform the followings in this child process:
//          close the write end descriptor of the pipe invoking close() system call
//          copy the read end  descriptor of the pipe to standard input file descriptor (STDIN_FILENO) invoking dup2() system call
//          change the process image of the this child with the new image according to the second UNIX command after the pipe symbol (|) using execvp() system call
//      close the read end descriptor of the pipe invoking close() system call
//      copy the write end descriptor of the pipe to standard output file descriptor (STDOUT_FILENO) invoking dup2() system call
//      change the process image with the new process image according to the first UNIX command before the pipe symbol (|) using execvp() system call
//If command does not conatain & (ampersand) at the end invoke wait() system call in parent process
void pipeArgument(char *arr[], int len, int mrk, bool ampFlag){

            //Create our two child processes
            pid_t pid1;
            pid_t pid2;

            int myPipeFd[2]; //pipe file descriptor: 0 for reading, 1 for writing
            int ret; //holds return value of executed system calls
            char *pre[mrk]; //store the first command separated from the pipe
            int mrk2 = (len - (mrk+1)); //length of second command 
            char *post[mrk2]; //holds the second command

            pid1 = fork();//fork first child process

            if(pid1 == 0){
                ///use pipe system call and store the return value
                //  if it returns -1, pipe failed
                ret = pipe(myPipeFd);
                if(ret == -1){
                    printf("The pipe failed initialization.\n");
                    return;
                }

                pid2 = fork();//fork second child

                if(pid2 == 0){

                    //fill post pipe array for execution
                    for(int i = 0; i < mrk2; i++){
                        post[i] = arr[i+mrk+1];
                    }
                    post[mrk2] = NULL; 

                    close(myPipeFd[1]); //make sure the pipe is closed from writing 
                    dup2(myPipeFd[0], STDIN_FILENO); //use dup2 to read using STDIN 
                    close(myPipeFd[0]);

                    ret = execvp(post[0], post); //execute the second command 
                    if(ret == -1){
                        printf("The second command failed execution.\n");
                    }

                } else {
                    //fill pre pipe array for execution
                    for(int i = 0; i < mrk; i++){
                        pre[i] = arr[i];
                    }
                    pre[mrk] = NULL; //set the marked pipe location as NULL

                    close(myPipeFd[0]); //make sure the pipe is closed from writing 
                    dup2(myPipeFd[1], STDOUT_FILENO);//use dup2 to write using STDOUT
                    close(myPipeFd[1]);

                    //execute the first command using execvp system call
                    ret = execvp(pre[0], pre);
                    if(ret == -1){
                        printf("The first command failed execution.\n");
                    }

                }
            } else {
                if(ampFlag == false){ //If the user did set as &, the parent will wait
                    waitpid(pid1,NULL,0);

                } else {
                    ampFlag = false; //otherwise, don't wait, just clear the flag.
                }
            }
}

//Helper function that will clear the contents of the array
void resetArray(char *arr[], int len){
    for(int i=0; i < len; i++){
        arr[i] = NULL;
    }
}
