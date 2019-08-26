#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>

#define FILENAME "config.txt"
#define CHECK_TIME 2
#define PING_TIME  1

typedef struct _process{
	char name[100];		// 进程名称
	long int lastPing;	// Ping时间
	int fd_read;
	int pid;		// 读管道
}process_t;

process_t process[100];	// 暂且设定为100
int len;	// 进程数量

void ping(int fd){
	struct timespec t;
	while(1){
		usleep(1000000 * PING_TIME);
		clock_gettime(CLOCK_REALTIME, &t);
		write(fd,&t.tv_sec,sizeof(long int));
		// printf("write %d\n", t.tv_sec);
	}
}

void* collectPing(void *arg){
	fd_set set;
	int i,maxfd;
	int fd;
	long int ping_sec;

	

	while(1){
		FD_ZERO(&set);
		maxfd = -1;
		for(i=0;i<len;++i){
			if(process[i].fd_read != -1){
				FD_SET(process[i].fd_read,&set);
				if(maxfd < process[i].fd_read)
					maxfd = process[i].fd_read;
			}
		}

		select(maxfd+1,&set,NULL,NULL,NULL); // i/o 多路转接

		for(i=0;i<len;++i){
			if(FD_ISSET(process[i].fd_read,&set)){
				int n = read(process[i].fd_read,&ping_sec,sizeof(long int));
				if(n != sizeof(long int)){
					process[i].fd_read = -1;
				}else{
					process[i].lastPing = ping_sec;
					printf("\"%s\" 心跳 %d\n",process[i].name, process[i].pid/* process[i].lastPing*/);
				}
			}
		}
	}
}

void checkChildren(){
	int i;
	struct timespec t;
	clock_gettime(CLOCK_REALTIME, &t);
	for(i=0;i<len;++i){
		if(t.tv_sec - process[i].lastPing > 1){	// 可脏读
			pid_t pid;
			int fd[2];

			printf("进程\"%s\"心跳异常，", process[i].name);

			pipe(fd);

			if((pid = fork()) == 0){
				close(fd[0]);
				ping(fd[1]);
				
			}else if(pid > 0){
				close(fd[1]);
				process[i].fd_read = fd[0];
				
				printf("已重新启动！\n");
			}else{
				printf("启动失败！\n");
			}
		}
	}
}

int main(){
	FILE *fptr = fopen(FILENAME,"r");
	int i;

	if(fptr == NULL){
		printf("open configurator file error!\n");
		return 1;
	}

	// 读取配置
	fscanf(fptr,"%d",&len);
	for(i=0;i<len;++i){
		char name[100];
		int no;
		fscanf(fptr,"%s%d",name,&no);
		if(no < 1 || no > len){
			printf("%s %d 编号错误！\n", name,no);
			continue;
		}else if(process[no-1].name[0] != '\0'){
			printf("%s %d 编号重复！\n", name,no);
			continue;
		}
		strcpy(process[no-1].name,name);
		process[no-1].lastPing = 0;
	}

	// 创建子进程以及通信用的管道
	for(i=0;i<len;++i){
		pid_t pid;
		int fd[2];

		pipe(fd);

		if((pid = fork()) == 0){
			close(fd[0]);
			ping(fd[1]);
		}else if(pid > 0){
			close(fd[1]);
			process[i].fd_read = fd[0];
			printf("创建进程\"%s\"成功！\n", process[i].name);
		}else{
			printf("创建进程\"%s\"失败！\n", process[i].name);
			return 2;
		}
	}

	// 启动子线程，更新心跳
	pthread_t tid;
	pthread_create(&tid,NULL,collectPing,NULL);
	
	// 另一个进程定期检查
	while(1){
		sleep(CHECK_TIME);
		checkChildren();
	}

	return 0;
}
