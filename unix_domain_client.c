#include <sys/types.h> 
#include <sys/socket.h> 
#include <sys/un.h> 
#include <unistd.h> 
#include <stdlib.h> 
#include <stdio.h> 
#include <string.h>

#define CMD_SEND_MESSAGE "CMD_SEND_MESSAGE_QUEST"
#define CMD_CONNECT_REQUEST "CMD_CONNECT_QUEST"
#define CMD_CLOSE_CONNECT "CMD_CLOSE_CONNECT"

int main(int argc,char **argv) 
{ 
		int sockfd = socket(AF_UNIX, SOCK_STREAM, 0); 
		struct sockaddr_un address; 
		address.sun_family = AF_UNIX; 
		strcpy(address.sun_path, "/var/log/sms_server"); 
		
		int result = connect(sockfd, (struct sockaddr *)&address, sizeof(address)); 
		if(result == -1) 
		{ 
				perror("connect failed: "); 
				exit(1); 
		} 
		char ch[1000];
		strcpy(ch,CMD_CONNECT_REQUEST); 
		write(sockfd, &ch, strlen(ch));
		memset(ch,0,sizeof(ch));
		read(sockfd, &ch, sizeof(ch)); 
		printf("get char from server: %s\n", ch); 
		while(1)
		{
			memset(ch,0,sizeof(ch));
			if(read(sockfd, &ch, sizeof(ch))>0)
			{
				printf("client receive:\n%s\n",ch);
			}
		}
		close(sockfd); 
		return 0; 
} 
