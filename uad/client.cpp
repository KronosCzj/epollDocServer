#include"define.h"
#include<string>
#define IPADDRESS "127.0.0.1"
#define CLI_PATH "/home/kronos/c++/epollDocServer/"	//客户端默认存放文件的路径名
using namespace std;


int update(char filename[],int connfd);			//客户端上传文件 
int download(char filename[],int connfd);      //客户端从服务器下载文件 

int update(char filename[],int connfd){			//客户端上传文件 
	package pack;
	FILE* fp;

	if((fp=fopen(filename,"rb"))==NULL){
		printf("cannot open file!\n");
		return -1;
	}
	printf("openfile ok!\n");	

  memcpy(pack.filename,filename,sizeof(pack.filename));
	int length;
	char *pBuff=new char[RCV_SIZE];
	while(!feof(fp)){
		length=fread(pack.buf,sizeof(char),BUF_SIZE,fp);
		pack.size=length;
		pack.fd=connfd;
		int iLen=0;
		*(int*)(pBuff+iLen)=(int)htonl(0); //host to net long ，上传文件type=0 
		iLen+=sizeof(int);
		*(int*)(pBuff+iLen)=(int)htonl(pack.size); //host to net long 
		iLen+=sizeof(int);
		*(int*)(pBuff+iLen)=(int)htonl(pack.fd); //host to net long 
		iLen+=sizeof(int);	
		memcpy(pBuff+iLen,pack.filename,NAME_SIZE);
		iLen+=sizeof(pack.filename);
		memcpy(pBuff+iLen,pack.buf,BUF_SIZE);
		iLen+=sizeof(pack.buf);
		ssize_t writeLen=MySend(connfd,pBuff,iLen);
		if(writeLen<0){
			printf("write failed\n");
			close(connfd);
			return -1;
		}
		memset(pBuff,0,sizeof(pBuff));
	}
	
	delete[] pBuff;
	fclose(fp);
	return 0;
}


int download(char filename[],int connfd){	 //从服务器下载文件 返回0成功
	char *recvMsg=new char[RCV_SIZE];
	int readLen;
	FILE* fp;
	package pack;
	
	string path=CLI_PATH;
	path+=filename;
	const char *p=path.c_str();
	if((fp=fopen(p,"ab"))==NULL){
		printf("cannot open file");
		return -1;
	}
	
	int count=0;
	while((readLen=MyRecv(connfd,recvMsg,RCV_SIZE))>0){
	//	printf("num:%d readlen:%d\n",++count,readLen);
		int iLen=0;
	
		memcpy(&pack.type,recvMsg+iLen,sizeof(int));				//开始解包 
		pack.type=(int)ntohl(pack.type);
		iLen+=sizeof(int);
		
		memcpy(&pack.size,recvMsg+iLen,sizeof(int));
		pack.size=ntohl(pack.size);
//		printf("pack.type:%d pack.size:%d\n",pack.type,pack.size);
		iLen+=sizeof(int);
		
		memcpy(&pack.fd,recvMsg+iLen,sizeof(int));
		pack.fd=ntohl(pack.fd);
		iLen+=sizeof(int);
	
		memcpy(&pack.filename,recvMsg+iLen,sizeof(pack.filename));
		iLen+=sizeof(pack.filename);
		
		memcpy(&pack.buf,recvMsg+iLen,sizeof(pack.buf));
		iLen+=sizeof(pack.buf);

		if(pack.size>0)
{
			fwrite(pack.buf,sizeof(char),pack.size,fp);
			fflush(fp);
		}
		if(pack.size<BUF_SIZE)
			break;
	}
	
	fclose(fp);
	return 0;
}

int main(int argc,char* argv[]){
	int connfd;
	struct sockaddr_in servaddr;
	connfd=socket(AF_INET,SOCK_STREAM,0);
	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family=AF_INET;
	servaddr.sin_port=htons(SERV_PORT);
	inet_pton(AF_INET,IPADDRESS,&servaddr.sin_addr);
	if(connect(connfd,(struct sockaddr*)&servaddr,sizeof(servaddr))<0){
		perror("connect error:");
		return -1;
	}else{
		printf("connect successfully\n");
	}

//struct timeval timeout={3,0};
//setsockopt(connfd,SOL_SOCKET,SO_RCVTIMEO,(const char*)&timeout,sizeof(struct timeval));

// fcntl(connfd,F_SETFL,O_NONBLOCK);		//将socket设置为非阻塞
 char opera[OPR_SIZE],com_file[NAME_SIZE];	//分别存放操作和完整的路径名（文件名）
	printf("input your opeartion:");
	scanf("%s %s",opera,com_file);
  
	if(strcmp(opera,"update")==0){ 	//update 要上传文件给服务器
		int type=0;
		char *st=new char[4];
		*(int*)st=htonl(type);
		MySend(connfd,st,sizeof(int));			//发送操作类型给服务器 
		if(update(com_file,connfd)==0)
			printf("sended all packages!\n");
		else
			printf("return -1!\n");	
	}else if(strcmp(opera,"download")==0){	//download 要下载文件
		int type=1;	
 		char *st=new char[4];
 		 *(int*)st=htonl(type);
		MySend(connfd,st,sizeof(int));		//发送操作类型给服务器
		int iLen=0;
		char *pBuff=new char[RCV_SIZE];		 	
	//	printf("sizeof(pBuff):%d",sizeof(pBuff));
		memset(pBuff,0,sizeof(pBuff));		
		*(int*)(pBuff+iLen)=(int)htonl(1); //这里还要再封包，再发送一个包告诉服务器端要下载哪个文件 
		iLen+=sizeof(int);
		*(int*)(pBuff+iLen)=(int)htonl(0); //host to net long 
		iLen+=sizeof(int);
		*(int*)(pBuff+iLen)=(int)htonl(connfd); //host to net long 
		iLen+=sizeof(int);	
		memcpy(pBuff+iLen,com_file,NAME_SIZE);
		iLen+=sizeof(com_file);
	//	printf("connfd:%d  iLen:%d sizeof(pBuff):%d\n",connfd,iLen,RCV_SIZE);
		if((iLen=MySend(connfd,pBuff,RCV_SIZE))>0)
			printf("send ok!  len:%d\n",iLen);
		else
			printf("send failed!\n");
		if(download(com_file,connfd)==0){		//从服务器接收文件 
				printf("download successfully\n");
			} 
	}else{
			printf("illegal operation!\n");
	}

	close(connfd);
	return 0; 
}
