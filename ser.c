/* Server.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <linux/in.h>


#define MAX_BUFFER_SIZE 1024             //最大数据大小
#define FILE_NAME_MAX_SIZE 512       //最大文件名大小

static pthread_t thread;             //线程ID
static pthread_t threadClient[100];  //客户线程数组，保存登陆状态用户
static int ServerSocket;               //服务端socket ID
static int clientNumber;             //客户数量
static int isFileSending;         //判断是否进行文件传输

//客户端数据结构 
typedef struct
{
	pthread_t threadNumber;//线程ID
	int sock;//socket ID
	char UserName[16];//用户名
	struct sockaddr address;//用户地址
	int addr_len;
} Connection;

static Connection connet_ary[100]; //保存用户的数组

//发送消息函数  Message 是需要分发的消息 except_user是不发的客户
int SendMessage(void* Message, int except_user)
{
	char *info = Message;
	for(int i = 0; i < 100; ++i)
		//发送给在线并且不在excetpion内的客户
		if(connet_ary[i].addr_len != -1 && connet_ary[i].addr_len != 0 && connet_ary[i].sock != except_user){
			if(send (connet_ary[i].sock, info , strlen(info) + 1, 0) == -1)
				printf("fail to send to %s fail", connet_ary[i].UserName);
			if(isFileSending == 0)
				printf("send <%s> to <%s>\n", info, connet_ary[i].UserName);
		}	
	return 0;	
}

//将客户端的文件发送给其他客户端
int SendFile(Connection* clientStruct)
{
	int size;
	int filesize;
	char buffer[1024];
	int len;
	isFileSending = 1;

	//获取文件的大小
	read(clientStruct->sock, &size, sizeof(int));
        read(clientStruct->sock, &filesize, sizeof(int));

	//先将将文件大小发送给所有其他客户端
	//发送文件大小字符串长度和字符串到客户端
	char filesizeString[20];
	char SizeOfFilesizeString[2];
	sprintf(filesizeString, "%d", filesize);
	sprintf(SizeOfFilesizeString, "%ld", strlen(filesizeString));
	SendMessage(SizeOfFilesizeString, clientStruct->sock);
	SendMessage(filesizeString, clientStruct->sock);
	
	//send file by parts
	for(int i=0; i < filesize/1024+1; ++i)
	{
		read(clientStruct->sock, &len, sizeof(int));
		read(clientStruct->sock, buffer, len);
		printf("receive %ld bytes\n", strlen(buffer));
		SendMessage(buffer, clientStruct->sock);
		printf("send file part %d successful!\n", i + 1);
		bzero(buffer, MAX_BUFFER_SIZE);
	}
	
	//print success message and return
	printf("send all parts successfully!\n");	
	isFileSending = 0;
	return 0;
}

//接收来客户端的消息
void* Receive(void* clientStruct)
{

	Connection* clientInfo = (Connection *)clientStruct;
	while(1)
	{
		//如果服务器正在发送文件，则不会收到的信息
		if(isFileSending) continue;
		//正常接受信息
		char *Buffer;
		int messageLen = 0;
		read(clientInfo->sock, &messageLen, sizeof(int));  

		if(messageLen > 0)
		{
			Buffer = (char *)malloc((messageLen+1)*sizeof(char));
			read(clientInfo->sock, Buffer, messageLen); 
						
			if(Buffer[0] != ':') continue;
			Buffer[messageLen] = '\0';
			//客户端想退出
			if( Buffer[1] == 'q' && Buffer[2] == '!' )
			{
				//发送退出消息，删除客户
				char quit[] = " 退出聊天室\n";
				char quitMessage[50];		
				char quitNumber[50];
				quitMessage[0] = '\0';
				sprintf(quitNumber, "现在聊天室人数有%d人\n", --clientNumber);
				strcat(quitMessage, clientInfo->UserName);
				strcat(quitMessage, quit);	
				strcat(quitMessage, quitNumber);
				//将信息发送
				SendMessage(quitMessage, -1);
				clientInfo->addr_len = -1;
				pthread_exit(&clientInfo->threadNumber);
			}
			else if ( Buffer[1] == 'f' && Buffer[2]  =='w')
			{	
				//发送文件
				char sign[] = "!!";
                char file[] = " send you a file named ";
				char fileMessage[50];
				char Filename[FILE_NAME_MAX_SIZE];
				fileMessage[0] = '\0';
				strcat(fileMessage, clientInfo->UserName);
				strcat(fileMessage, file);
				//读文件和转发文件
				for(int t = 4; t < messageLen-1; t++)
					Filename[t-4] = Buffer[t];
				Filename[messageLen-5]='\0';
				strcat(fileMessage, Filename);
				strcat(sign, fileMessage);
				SendMessage(sign, -1);
				SendFile(clientInfo);
			}
			else if ( Buffer[1] == 'm' && Buffer[2]  =='v')
			{	
				//send the value of media DealThread to the others
				SendMessage(Buffer, clientInfo->sock);
			}
			else{
				//constitute the message
				char begin[] = " 说";
				char messageDistribute[200];
				messageDistribute[0] = '\0';
				strcat(messageDistribute, clientInfo->UserName);
				strcat(messageDistribute, begin);
				strcat(messageDistribute, Buffer);
				SendMessage(messageDistribute, -1);
			}
			free(Buffer);
		}
		else
			continue;
	}
}

//判断用户名是否存在
int isUsernameExisted(char userName[], int clientnumber)
{
	for(int i = 0; i < 100 && i != clientnumber; ++i)
	{
		if(connet_ary[i].addr_len != 0 && connet_ary[i].addr_len != -1)
			if(strcmp(connet_ary[i].UserName, userName) == 0)
				return 1;

	}	
	return 0;
}


//专门处理客户端连接的进程
void * DealThread(void * ptr)
{
	char* buffer;
	int len;
	clientNumber = 0;   //初始化     
	long addr = 0;
	//循环等待连接
	while(1){
		
		if(clientNumber < 100)
		{
			connet_ary[clientNumber].sock = accept(ServerSocket, &connet_ary[clientNumber].address, &connet_ary[clientNumber].addr_len);
		}
		else
			break;

		//读取消息的长度
		read(connet_ary[clientNumber].sock, &len, sizeof(int));
		//长度大于0 才处理
		if (len > 0)
		{
			
			addr = (long)((struct sockaddr_in *)&connet_ary[clientNumber].address)->sin_addr.s_addr;
			buffer = (char *)malloc((len+1)*sizeof(char));
			buffer[len] = '\0';
			read(connet_ary[clientNumber].sock, buffer, len);
			
			//保存客户名 
			strcpy(connet_ary[clientNumber].UserName, buffer);
		
			//判断是否重名
			if(isUsernameExisted(connet_ary[clientNumber].UserName, clientNumber))
			{
				send(connet_ary[clientNumber].sock,  "Reject", 6, 0);
				--clientNumber;
			}
			else
			{
				//发送登陆成功信息
				char login_buff[] = "欢迎您进入播放室，请开始愉快地看视频聊天吧!\n";
				send (connet_ary[clientNumber].sock, "欢迎您进入播放室，请开始愉快地看视频聊天吧!\n", strlen(login_buff), 0);
				
				//send inform message to all the users
				char MesHeader[50] = "用户 ";
				char MesMiddle[30] = " 已经进入了播放室!\n";
				char MesNumber[50];
				sprintf(MesNumber, "目前播放室有%d人\n", clientNumber + 1);
				strcat(MesHeader, connet_ary[clientNumber].UserName);
				strcat(MesHeader, MesMiddle);
				strcat(MesHeader, MesNumber);
				printf("%s", MesHeader);
				SendMessage(MesHeader, -1);
				
				//创建接受消息的进程
				pthread_create(&threadClient[clientNumber], 0, Receive, &connet_ary[clientNumber]);
				connet_ary[clientNumber].threadNumber = threadClient[clientNumber];
			}
			free(buffer);
		}
		clientNumber += 1;
		
	}
	pthread_exit(0);
}


int main(int argc, char ** argv){
	struct sockaddr_in address;
	int port = 8888;
	Connection* NewConnection;

	//创建socket
	ServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	//bind the socket
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	//错误信息
	if (bind(ServerSocket, (struct sockaddr *)&address, sizeof(struct sockaddr_in)) < 0)
	{
		fprintf(stderr, "错误: 不能绑定端口 %d\n", port);
		return -4;
	}

	//监听接口
	listen(ServerSocket, 100);
	printf("服务端启动成功 正在等待用户加入播放室\n");

	//创建专门处理连接的线程
	pthread_create(&thread, 0, DealThread, (void *)NewConnection);
	
	for(int i = 0; i < 100; ++i)
		sleep(10000);

	//关闭 socket and 线程
	pthread_detach(thread);
	close(ServerSocket);
	return 0;
}
