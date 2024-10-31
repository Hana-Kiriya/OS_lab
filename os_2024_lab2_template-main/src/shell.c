#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "../include/command.h"
#include "../include/builtin.h"

// ======================= requirement 2.3 =======================
/**
 * @brief 
 * Redirect command's stdin and stdout to the specified file descriptor
 * If you want to implement ( < , > ), use "in_file" and "out_file" included the cmd_node structure
 * If you want to implement ( | ), use "in" and "out" included the cmd_node structure.
 *
 * @param p cmd_node structure
 * 
 */
void redirection(struct cmd_node *p){ //文件重定向
	int fd;

	if(p -> in_file){ //讀取 p -> in_file
		fd = open(p -> in_file, O_RDONLY);

		if(fd == -1){
			perror("open input file");
			exit(EXIT_FAILURE);
		}
		if(dup2(fd, STDIN_FILENO) == -1){ //將標準輸入STDIN_FILENO重定向到fd
			perror("dup2 input");
			exit(EXIT_FAILURE);
		}
		close(fd);
	}

	if(p -> out_file){ //輸出到 out_file
		fd = open(p -> out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);

		if(fd == -1){
			perror("open output file");
			exit(EXIT_FAILURE);
		}
		if(dup2(fd, STDOUT_FILENO) == -1){ //將標準輸出STDOUT_FILENO重定向到fd
			perror("dup2 output");
			exit(EXIT_FAILURE);
		}
		close(fd);
	}
}
// ===============================================================

// ======================= requirement 2.2 =======================
/**
 * @brief 
 * Execute external command
 * The external command is mainly divided into the following two steps:
 * 1. Call "fork()" to create child process
 * 2. Call "execvp()" to execute the corresponding executable file
 * @param p cmd_node structure
 * @return int 
 * Return execution status
 */
int spawn_proc(struct cmd_node *p) //執行單一外部命令
{
	pid_t pid = fork(); //創建子進程
	pid_t wpid;
	int status;

	if(pid == -1){ //創建子進程失敗
		perror("fork");
	}
	else if(pid == 0){ //處於子進程
		redirection(p); //重定向
		if(execvp(p -> args[0], p -> args) == -1){ //呼叫execvp執行外部命令
			perror("execvp");
			exit(EXIT_FAILURE);
		}
	}
	else{ //處於父進程
		do{
			wpid = waitpid(pid, &status, 0); //父進程等待子進程結束
		}while(!WIFEXITED(status) && !WIFSIGNALED(status)); //檢查是否正常退出或被訊號中止
		//子進程正常退出時，WIFEXITED(status)會回傳true。
		//子進程因未捕獲的訊號中止時，WIFSIGNALED(status)回傳true。
		// => 子進程未正常退出 且 未被訊號中止時，會持續進入迴圈
	}
  	return 1;
}
// ===============================================================


// ======================= requirement 2.4 =======================
/**
 * @brief 
 * Use "pipe()" to create a communication bridge between processes
 * Call "spawn_proc()" in order according to the number of cmd_node
 * @param cmd Command structure  
 * @return int
 * Return execution status 
 */
int fork_cmd_node(struct cmd *cmd) //處理多個命令並串接管道
{
	struct cmd_node *current = cmd -> head;
	int pipe_fd[2];
	int in_fd = STDIN_FILENO;
	pid_t pid;
	int status;

	//歷遍cmd_node的鏈表 //current為一個命令
	while(current != NULL){
		//確認是否為最後一個節點，不是則用pipe創新管道
		if(current -> next != NULL){
			if(pipe(pipe_fd) == -1){
				perror("pipe");
				exit(EXIT_FAILURE);
			}
		}

		//創建子進程
		pid = fork();
		if(pid == -1){
			perror("fork");
			exit(EXIT_FAILURE);
		}
		else if(pid == 0){
			//子進程進行I/O重定向 //管道重定向
			if(in_fd != STDIN_FILENO){ //in_fd != STDIN_FILENO表示上一個指令的輸出已接至in_fd
				if(dup2(in_fd, STDIN_FILENO) == -1){ //在此將in_fd(上個命令的輸出) 作為此管道的標準輸入
					perror("dup2 input");
					exit(EXIT_FAILURE);
				}
				close(in_fd);
			}
			if(current -> next != NULL){ //若此命令非最後一個
				if(dup2(pipe_fd[1], STDOUT_FILENO) == -1){ //則將當前命令的標準輸出設為當前管道的寫入端，供下個命令使用
					perror("dup2 output");
					exit(EXIT_FAILURE);
				}
				close(pipe_fd[1]);
				close(pipe_fd[0]);
			}

			redirection(current); //文件重定向

			if(execvp(current -> args[0], current -> args) == -1){ //呼叫execvp執行命令
				perror("execvp");
				exit(EXIT_FAILURE);
			}
		}
		else{
			//父進程在每個命令執行完後，確認若fd 非 STDIN_FILENO，則關閉in_fd以釋放資源
			if(in_fd != STDIN_FILENO){
				close(in_fd);
			}
			if(current -> next != NULL){ //若非最後一個命令
				close(pipe_fd[1]); //則關閉寫入端
				in_fd = pipe_fd[0]; //將當前管道的讀取端傳給下一個命令的in_fd
			}

			current = current -> next; //移動到下個命令
		}
	}

	while((pid == wait(&status)) > 0); //等所有子進程結束
	
	return 1;
}
// ===============================================================


void shell()
{
	while (1) {
		printf(">>> $ ");
		char *buffer = read_line();
		if (buffer == NULL)
			continue;

		struct cmd *cmd = split_line(buffer);
		
		int status = -1;
		// only a single command
		struct cmd_node *temp = cmd->head;
		
		if(temp->next == NULL){
			status = searchBuiltInCommand(temp);
			if (status != -1){
				int in = dup(STDIN_FILENO), out = dup(STDOUT_FILENO);
				if( in == -1 | out == -1)
					perror("dup");
				redirection(temp);
				status = execBuiltInCommand(status,temp);

				// recover shell stdin and stdout
				if (temp->in_file)  dup2(in, 0);
				if (temp->out_file){
					dup2(out, 1);
				}
				close(in);
				close(out);
			}
			else{
				//external command
				status = spawn_proc(cmd->head);
			}
		}
		// There are multiple commands ( | )
		else{
			
			status = fork_cmd_node(cmd);
		}
		// free space
		while (cmd->head) {
			
			struct cmd_node *temp = cmd->head;
      		cmd->head = cmd->head->next;
			free(temp->args);
   	    	free(temp);
   		}
		free(cmd);
		free(buffer);
		
		if (status == 0)
			break;
	}
}
