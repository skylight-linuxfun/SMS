#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <error.h>
#include <termios.h>
#include <fcntl.h>
#include <malloc.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include<sys/select.h>
#include<sys/time.h>
#include<sys/shm.h>

#define SOCKET_FILE "/var/log/sms_server"
#define CENTER_NUMBER "+8613010888500"
#define UART_DEV_NAME "/dev/ttyUSB0"
#define GSM_LOG_PATH "/var/log/gsm.txt"
#define MAX_BUFFER_SIZE 512
#define bool unsigned char
#define size_t unsigned char
#define true 1
#define false 0

#define CMD_SEND_MESSAGE "CMD_SEND_MESSAGE_QUEST"
#define CMD_CONNECT_REQUEST "CMD_CONNECT_QUEST"
#define CMD_CONNECT_OK "Welcome"
#define CMD_CLOSE_CONNECT "CMD_CLOSE_CONNECT"
#define CMD_CLOSE_OK "Bye-Bye"

typedef struct{
	size_t year;
	size_t month;
	size_t day;
	size_t hour;
	size_t minute;
	size_t second;
}time_info_t;

typedef struct {
	char center_number[20];
	char send_number[20];
	char send_time[20];
	char send_content[MAX_BUFFER_SIZE];
}message_info_t;

typedef struct{
	int fd;
	bool isConnected ;
	bool isStop;
	pthread_t threadID;
	
}new_connect;

typedef struct {
  int fd;
  bool isReceivedMsg;
  int shmid;
  pthread_mutex_t db;
}globals;

char rcv_msg_sim_index[3] = {0};
globals global;

bool sendMessage(const char *to,const char *msg);
void number_handler(char *to);
void center_next_handler(char *center);
void uart_config(void);
bool write_to_uart(char *final,int length);
void *listen_client_connect_thread_func(void *arg);
void gsm_init(void);
void substring(const char *src,char *dest,int index,int length);
bool save_gsm_log(message_info_t history_info);
message_info_t handler_receive_msg(char *msg);
void *new_conn_handler(void *arg);

int main(int argc,char **argv)
{	
	uart_config();
	gsm_init();
	sendMessage("18664307310", "4E2D");
	global.isReceivedMsg = false;
	pthread_mutex_init(&global.db, NULL);
	if((global.shmid=shmget(IPC_PRIVATE,MAX_BUFFER_SIZE,0666))<0)
	{
		perror("create share-memory");
		exit(1);
	}
	//create receive message Thread
	pthread_t listenClientThreadID;
	//pthread_create(&listenClientThreadID,NULL,listen_client_connect_thread_func,NULL);
	//pthread_detach(listenClientThreadID);

	int read_length = 0;
	char buffer[MAX_BUFFER_SIZE]={0};
	char command[MAX_BUFFER_SIZE] ={0};
	char *share_msg_info_addr;
	
	if((share_msg_info_addr = shmat(global.shmid,0,0))==(void *)-1)
	{
		perror("Parent: shmat:");
		exit(1);
	}
	
	while(true)
	{			
		if(global.isReceivedMsg)
		{
			sendMessage("18664307310", "4E2D");
			global.isReceivedMsg = false;
		}
		if((read_length=read(global.fd,buffer,sizeof(buffer)))>0)
		{
			if(strstr(buffer,"0891"))
			{
				// 1. get the messgae info
				message_info_t rcv_info = handler_receive_msg(strstr(buffer,"0891"));
				// 2.share memory
				memset(buffer,0,sizeof(buffer));
				sprintf(buffer,"%s,%s,%s\0",rcv_info.send_number,rcv_info.send_time,rcv_info.send_content);
				memset(share_msg_info_addr,0,sizeof(share_msg_info_addr));
				memcpy(share_msg_info_addr,buffer,strlen(buffer));
				// 3.save log
				save_gsm_log(rcv_info);
				// 4.delete it from SIM
				memset(buffer,0,sizeof(buffer));
				sprintf(buffer,"AT+CMGD=%s\r\0",rcv_msg_sim_index);
				write(global.fd,buffer,strlen(buffer));
				global.isReceivedMsg = true;
			}
			else if(strstr(buffer,"CMTI:"))//new message is coming
			{
				memset(command,0,sizeof(command));
				strcpy(command,buffer);
				printf("new message : %s\n",buffer);
				memset(buffer,0,sizeof(buffer));
				strtok(command,",");
				strcpy(rcv_msg_sim_index,strtok(NULL,","));
				sprintf(buffer,"AT+CMGR=%s\r\0",rcv_msg_sim_index);
				write(global.fd,buffer,strlen(buffer));  //read new message command
			}
			else
			{
				printf("buffer is: %s\n",buffer);
			}
			memset(buffer,0,sizeof(buffer));
		}
	}
	return 0;
}

void *listen_client_connect_thread_func(void *arg)
{
	unlink(SOCKET_FILE);
	new_connect new_conn;
	int server_fd = socket(AF_UNIX,SOCK_STREAM,0);
	struct sockaddr_un server_addr,client_addr;
	server_addr.sun_family = AF_UNIX;
	strcpy(server_addr.sun_path,SOCKET_FILE);

	bind(server_fd,(struct sockaddr *)&server_addr,sizeof(server_addr));
	listen(server_fd,10);
	int len = sizeof(client_addr);

	while(true)
	{
		int client_fd = accept(server_fd,(struct sockaddr *)&client_addr,&len);
		new_conn.fd = client_fd;
		new_conn.isConnected = false;
		new_conn.isStop = false;
		pthread_create(&(new_conn.threadID),NULL,new_conn_handler,&new_conn);
		pthread_detach(new_conn.threadID);
	}
}

void *new_conn_handler(void *arg)
{
	char socket_buffer[MAX_BUFFER_SIZE]={0};
	new_connect *new_conn = arg;
	fd_set fds;
	struct timeval timeout={0,0};
	int maxfdp = new_conn->fd + 1;

	char *share_msg_info_get_addr;
	
	if((share_msg_info_get_addr = shmat(global.shmid,0,0))==(void *)-1)
	{
		perror("Parent: shmat:");
		exit(1);
	}
	
	while(!new_conn->isStop)
	{
		pthread_mutex_lock(&global.db);
		memset(socket_buffer,0,sizeof(socket_buffer));
		if(global.isReceivedMsg == true && new_conn->isConnected == true)
		{
			memset(socket_buffer,0,sizeof(socket_buffer));
			memcpy(socket_buffer,share_msg_info_get_addr,strlen(share_msg_info_get_addr));
			write(new_conn->fd,socket_buffer,strlen(socket_buffer));
			global.isReceivedMsg = false;
		}
		FD_ZERO(&fds);
		FD_SET(new_conn->fd,&fds);
		switch(select(maxfdp,&fds,NULL,NULL,&timeout))
		{
			case -1:
				break;
			case 0:
				break;
			default:
				if(FD_ISSET(new_conn->fd,&fds))
				{
					memset(socket_buffer,0,sizeof(socket_buffer));
					read(new_conn->fd,&socket_buffer,sizeof(socket_buffer));
					if(new_conn->isConnected == false)
					{
						if(strstr(socket_buffer,CMD_CONNECT_REQUEST))
						{
							printf("new connect\n");
							write(new_conn->fd,CMD_CONNECT_OK,sizeof(CMD_CONNECT_OK));
							new_conn->isConnected = true;
						}
					}
					else
					{
						if(strstr(socket_buffer,CMD_SEND_MESSAGE))
						{
							
							printf("send Message\n");
							strtok(socket_buffer,",");
							char *mobile_number = strtok(NULL,",");
							char *msg_content = strtok(NULL,",");
							if(mobile_number!=NULL && msg_content!=NULL)
							{
								printf("receive_number: %s msg_content: %s\n",mobile_number,msg_content);
								if(sendMessage(mobile_number, msg_content))
									write(new_conn->fd,"send OK",7);
								else
									write(new_conn->fd,"send Fail",9);
							}
						}
						else if(strstr(socket_buffer,CMD_CLOSE_CONNECT))
						{
							 new_conn->isStop = true;
							 write(new_conn->fd,CMD_CLOSE_OK,sizeof(CMD_CLOSE_OK));
						}
					}
				}
		}
		pthread_mutex_unlock(&global.db);
	}
	close(new_conn->fd);
}

message_info_t handler_receive_msg(char *src)
{
	message_info_t rcv_info_t;
	time_info_t time_info;
	char t[2] = {0};
	char len[2] = {0};
	int length = 0,pre_length = 0;
	//get center number
	substring(src,rcv_info_t.center_number,6,12);
	number_handler(rcv_info_t.center_number);
	rcv_info_t.center_number[11] = '\0';
	//get send number
	substring(src,len,20,2);
	length = strtol(len,NULL,16);
	pre_length = length;
	if(pre_length%2) length+=1;
	if(*(src+22)=='9' && *(src+23)=='1')
	{
		substring(src,rcv_info_t.send_number,26,length-2);
	}
	else
	{
		substring(src,rcv_info_t.send_number,24,length);
	}
	number_handler(rcv_info_t.send_number);
	if(pre_length%2) 
	{
		if(*(src+22)=='9' && *(src+23)=='1')
		{
			rcv_info_t.send_number[pre_length-2] = '\0';
		}
		else
		{
			rcv_info_t.send_number[pre_length] = '\0';
		}
	}
	//get send time
	substring(src,rcv_info_t.send_time,28+length,12);
	number_handler(rcv_info_t.send_time);
	substring(rcv_info_t.send_time, t, 0, 2);
	time_info.year = atoi(t);
	substring(rcv_info_t.send_time, t, 2, 2);
	time_info.month= atoi(t);	
	substring(rcv_info_t.send_time, t, 4, 2);
	time_info.day = atoi(t);
	substring(rcv_info_t.send_time, t, 6, 2);
	time_info.hour = atoi(t);
	substring(rcv_info_t.send_time, t, 8, 2);
	time_info.minute = atoi(t);
	substring(rcv_info_t.send_time, t, 10, 2);
	time_info.second = atoi(t);
	sprintf(rcv_info_t.send_time,"20%.2d-%.2d-%.2d %.2d:%.2d:%.2d\0",
		time_info.year,time_info.month,time_info.day,time_info.hour,time_info.minute,time_info.second);
	//get content length
	substring(src, len, 42+length, 2);	
	//get content
	substring(src,rcv_info_t.send_content,44+length,strtol(len,NULL,16)*2);
	return rcv_info_t;
}

void uart_config(void)
{
	struct termios options;
	if((global.fd =open(UART_DEV_NAME,O_RDWR|O_NOCTTY|O_NDELAY))<0){
		close(global.fd);
		perror("open "UART_DEV_NAME);
		exit(1);
	}
	tcgetattr(global.fd, &options);
	tcflush(global.fd, TCIOFLUSH);
	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	cfsetispeed(&options, B9600);
	cfsetospeed(&options, B9600);
 	options.c_cflag &= ~CSIZE;
 	options.c_cflag |= CS8;
 	options.c_cflag &= ~PARENB;
 	options.c_cflag &= ~INPCK;
 	options.c_cflag &= ~CSTOPB;
 	options.c_cc[VTIME] = 250;
 	options.c_cc[VMIN] = 0;
 	tcsetattr(global.fd, TCSANOW, &options);
 	tcflush(global.fd, TCIOFLUSH);
}

void gsm_init(void)
{
	char buffer[MAX_BUFFER_SIZE]={0};
	//close echo back
	strcpy(buffer,"ATE0\r");
	write(global.fd, buffer , strlen(buffer));
	memset(buffer, 0 ,sizeof(buffer));
	//set tip way
	strcpy(buffer,"AT+CNMI=2,1,0,2,1\r");
	write(global.fd, buffer , strlen(buffer));
	memset(buffer, 0 ,sizeof(buffer));
	//set store way
	strcpy(buffer,"AT+CPMS=\"SM\",\"SM\",\"SM\"\r");
	write(global.fd, buffer,strlen(buffer));
	memset(buffer, 0 ,sizeof(buffer));
	//set PDU mode
	strcpy(buffer,"AT+CMGF=0\r");
	write(global.fd,buffer,strlen(buffer));
	printf("init OK!\n");
}

bool sendMessage(const char *_to,const char *_msg)
{
	char to[20];
	char center[20];
	char msg[MAX_BUFFER_SIZE];
	char final[MAX_BUFFER_SIZE];
	//handle receive number
	memset(to,0,sizeof(to));
	sprintf(to,"+86%s",_to);
	number_handler(to);

	//handle center number
	memset(center,0,sizeof(center));
	strcpy(center,CENTER_NUMBER);
	number_handler(center);
	center_next_handler(center);

	//handles msg
	memset(msg,0,sizeof(msg));

	sprintf(msg,"%.2X%s",strlen(_msg)/2,_msg);
	//zuhe
	memset(final,0,sizeof(final));
	sprintf(final,"1100%.2X91%s000800%s\0",strlen(_to)+2,to,msg);
	printf("final : %s\n",final);

	int length = strlen(final)/2;
	printf("length is %.2d\n",length);

	//write to uart
	if(write_to_uart(final,length)){
			return true;
	}
	return false;
}

bool write_to_uart(char *final,int length)
{
	char buffer[MAX_BUFFER_SIZE]={0};
	//send message length
	sprintf(buffer,"AT+CMGS=%.2d\r",length);
	write(global.fd,buffer,strlen(buffer));
	memset(buffer, 0, sizeof(buffer));
	//send message content
	sprintf(buffer,"%s\x1a",final);
	write(global.fd,buffer,strlen(buffer));
	memset(buffer, 0, sizeof(buffer));
	read(global.fd, buffer, MAX_BUFFER_SIZE);
	printf("send result: %s\n",buffer);
	if(strstr(buffer,"OK"))
		{return true;}
	return false;
}

void number_handler(char *to)
{
	unsigned int i = 0,str_len=0;
	char tmp;
	if(*to == '+'){
		for(i = 0;i <(strlen(to)-1);i++){
				*(to + i) = *(to + i +1);
		} 
		*(to + i) = '\0';
	}	
	str_len = strlen(to);
	if(str_len%2==1){
		*(to + str_len) = 'F';
		*(to + str_len + 1) = '\0';
	}
	for(i = 0;i < strlen(to);i+=2){
		tmp = *(to + i);
		*(to + i) = *(to + i + 1);
		*(to + i + 1) = tmp;	
	}
}

void center_next_handler(char *center)
{
	unsigned int str_len = 0,i = 0,tmp = 0;
	str_len = strlen(center);
	for(i = str_len+2;i>=2;i--){
		*(center+i) = *(center+i-2);
	}
	*(center+str_len+3) = '\0';
	strncpy(center,"91",2);

	tmp = strlen(center)/2;
	
	str_len = strlen(center);
	for(i = str_len+2;i>=2;i--){
		*(center+i) = *(center+i-2);
	}
	*(center+str_len+3) = '\0';
	*center = (char)(tmp/10) + 0x30;
	*(center + 1) = (char)(tmp%10) + 0x30;
}

void substring(const char *src,char *dest,int index,int length)
{
	int count = 0;
	while(count<length){*(dest++) = *(src + index++);count++;}
	*(dest++) = '\0';
}

bool save_gsm_log(message_info_t history_info)
{
	FILE *fid = fopen(GSM_LOG_PATH,"at+");
	char line[1024] = {0};
	printf("_______________________________________________________________________________________________\n");
	printf("center number: %s\n",history_info.center_number);
	printf("send number : %s\n",history_info.send_number);
	printf("send time : %s\n",history_info.send_time);
	printf("send content : %s\n",history_info.send_content);
	printf("________________________________________________________________________________________________\n");
	sprintf(line,"%-20s%-20s%-25s%s\n\0",
		history_info.center_number,history_info.send_number,history_info.send_time,history_info.send_content);
	if(!fid){printf("open "GSM_LOG_PATH" error!\n");return false;}
	fwrite(line,strlen(line),1,fid);
	fclose(fid);
	return true;
}
