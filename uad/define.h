#include<stdio.h>
#include<sys/epoll.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<fcntl.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<stdlib.h>
#include<time.h>
#include<list>
#include<pthread.h>
#pragma pack(1)  //单字节对齐

#define OPR_SIZE 20
#define MAX_EVENTS 1024
#define BUF_SIZE 4096
#define SERV_PORT 6666
#define LISNUM 100
#define NAME_SIZE 128
#define POOL_NUM 5
#define RCV_SIZE BUF_SIZE+NAME_SIZE+24
using namespace std;

struct my_event{		
	int fd;												//文件描述符 
	int events;											//对应的监听事件 
	void *arg;											//泛型参数 ，用于指向结构体自己 
	void (*call_back)(int fd,int events,void *arg);		//回调函数
	int status;											//是否在监听：1表示在红黑树上（在监听） 0表示不在树上（不在监听）
	long last_active;									//上一次活跃的时间								
};


struct package{
	int type;			//0表示客户端上传数据（服务器要读数据），1表示客户端下载数据（服务器要写数据） 
	int size;  			//本次传输的文件内容的大小
	int fd;				//文件描述符 
	char filename[NAME_SIZE];	//文件名 
	char buf[BUF_SIZE];  	//文件内容 
}; 

struct node{		//任务列表中的结点 
	char buf[RCV_SIZE];
};

int g_efd;
struct my_event g_events[MAX_EVENTS+1];
//queue<package> packtask;
list<node> packtask;			//任务列表 
pthread_mutex_t t_mutex;		//任务列表的互斥锁 

int MySend(int iSock,char* pchBuf,size_t tLen){	//把pchBuf里tLen长度数据发送出去，直到发送完为止 
	int iThisSend; //一次发送了多少数据 
	unsigned int iSended=0;		//已经发送了多少 
	if(tLen==0)
		return 0;
	while(iSended<tLen){
		do{
			iThisSend=send(iSock,pchBuf,tLen-iSended,0);
		}while((iThisSend<0)&&(errno==EINTR));		//发送的时候遇到了中断 
		if(iThisSend<0){
		//	perror("Mysend error:");
			return iSended;
		}
		iSended+=iThisSend;
		pchBuf+=iThisSend;
	}
	return tLen;
} 

int MyRecv(int iSock,char* pchBuf,size_t tLen){
	int iThisRead;
	unsigned int iReaded=0;
	if(tLen==0)
		return 0;
	while(iReaded<tLen){
		do{
			iThisRead=recv(iSock,pchBuf,tLen-iReaded,0);
		}while((iThisRead<0)&&(errno==EINTR));
		if(iThisRead<0){
		//	perror("MyRecv error");
			return iThisRead;
		}
		else if(iThisRead==0)
			return iReaded;
		iReaded+=iThisRead;
		pchBuf+=iThisRead;
	}
}

void getOper(char a[],char c[]){	//将输入的要上传的文件路径分割出文件件名放在c数组中，例如 e://aaa.txt  c=aaa.txt 
	int i=0;
	for(;i<strlen(a)-1;i++){
		if((a[i]=='/'&&a[i+1]!='/')||(a[i]=='\\'&&a[i+1]!='\\'))
			break;
		else 
			c[i]=a[i];
//		cout<<i<<endl;
	}
	if(i==strlen(a)-1){
  //如果文件名里没有\ 或者/
		c[i]=a[i];
		return;
	}	
	int t=++i;
	for(;i<strlen(a);i++){
		c[i-t]=a[i];
		c[i+1]='\0';
	}
	c[i]='\0';
//	cout<<"c:"<<c<<endl;
}
 
