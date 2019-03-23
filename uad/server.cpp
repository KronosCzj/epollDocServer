#include"define.h"
#include"threadpool.h"
#include<string>
using namespace std;
#define PATH "/home/kronos/c++/getFile/"
threadpool_t pool;			//这个线程池负责：1.接受客户端上传的文件包并加到任务列表 	2.将客户端要下载的文件读成数据包并加入任务列表 
threadpool_t handler_pool;	//这个线程池负责：将任务队列里的包，按照包里的type和fd进行发送（客户端下载文件）或者写到本地文件（客户端上传文件） 

void recvdata(int fd,int events,void *arg);		//用来接收客户端先发的一个type来判定客户端要进行的操作 
//void senddata(int fd,int events,void *arg);
void initlistensocket(int efd,short port);		//初始化listenfd 
void eventset(struct my_event* ev,int fd,void (*call_back)(int ,int ,void *),void *arg);	//设置event 
void eventadd(int efd,int events,struct my_event* ev);	//将ev->fd加到g_efd红黑树上 ，即g_efd监听ev->fd 
void eventdel(int efd,struct my_event *ev);		//将ev->fd从g_efd红黑树上删除 ,即g_efd不监听ev->fd 
void acceptconn(int lfd,int events,void *arg); 	//接收连接 
void recvfile(void *arg);			// 客户端上传文件时，将接收的数据包加入任务列表 （线程池pool的任务函数） 
void readfilefromlocal(void *arg);		//客户端要下载文件时，用来从服务器端读文件，包装成数据包后加入任务列表 （线程池pool的任务函数） 
void handle(void *arg);		//将任务队列里的包，按照包里的type和fd进行发送（客户端下载文件）或者写到本地文件（客户端上传文件） （线程池handler_pool的任务函数）  

void recvdata(int fd,int events,void *arg){
	char *type=new char[4];
	int readLen;
	struct my_event *ev=(struct my_event*)arg;

 	printf("recvdate fd:%d\n",fd);
	readLen=MyRecv(ev->fd,type,sizeof(int));		//先从客户端接收type来判断客户端要进行的操作 
	if(readLen<0){
		printf("读取数据失败！\n");
		return;
	}
	int itype;
	memcpy(&itype,type,sizeof(int));
	itype=(int)ntohl(itype);
	if(itype==0){			// 客户端要进行上传操作 
		eventdel(g_efd,ev);		//删除ev->fd，否则客户端每发一个数据包就会触发一次这个函数 
		threadpool_add_task(&pool, recvfile, arg);		//线程池pool加任务，任务函数是recvfile 
	}
	else if(itype==1){		//客户端要进行下载操作 
		eventdel(g_efd,ev);
		threadpool_add_task(&pool, readfilefromlocal, arg);	//线程池pool加任务，任务函数是readfilefromlocal
	}

	delete[] type;
}

void readfilefromlocal(void *arg){
//	printf("excuting readfilefromlocal\n");
	char *recvMsg=new char[RCV_SIZE];
	struct my_event *ev=(struct my_event*)arg;
	int readlen;
	package pack;
	if((readlen=MyRecv(ev->fd,recvMsg,RCV_SIZE))<0)		//先接收一个客户端的一个包，来知道客户端要下载哪个文件 
	{
		perror("errno:");
		printf("fd:%d readlen:%d\n",ev->fd,readlen);
		return;
	}
//	printf("readlen:%d\n",readlen);
	int iLen=0;
	memcpy(&pack.type,recvMsg+iLen,sizeof(int));			//将包中数据段一个一个解析 
	pack.type=ntohl(pack.type);
//	printf("pack.tpye:%d\n",pack.type);
	if(pack.type!=1)
		return;
	iLen+=sizeof(int);
		
//	memcpy(&pack.size,recvMsg+iLen,sizeof(int));
//	pack.size=ntohl(pack.size);
	iLen+=sizeof(int);
	
	
	pack.fd=ntohl(ev->fd);
	iLen+=sizeof(int);

	memcpy(&pack.filename,recvMsg+iLen,sizeof(pack.filename));
	iLen+=sizeof(pack.filename);
//	printf("pack.name:%s\n",pack.filename);
	delete[] recvMsg;
	
	string path=PATH;									
	path+=pack.filename;
	//	printf("path: %s\n",path.c_str());
	const char *p=path.c_str();				//得到PATH+filename的完整路径 
	char *pBuff=new char[RCV_SIZE];
	struct node n;
	FILE *fp;
	int length;
	if((fp=fopen(p,"rb"))==NULL){		 
		printf("cannot open file!\n");
		return;
	}
	while(!feof(fp)){						//开始将客户端要下载的包包装成包并加入任务队列 
		memset(pack.buf,0,BUF_SIZE);
		length=fread(pack.buf,sizeof(char),BUF_SIZE,fp);
		pack.size=length;
	//	printf("file length:%d\n",length);
		int iLen=0;
		*(int*)(pBuff+iLen)=(int)htonl(pack.type); //host to net long 
		iLen+=sizeof(int);
		*(int*)(pBuff+iLen)=(int)htonl(pack.size); //host to net long 
		iLen+=sizeof(int);
		*(int*)(pBuff+iLen)=(int)htonl(pack.fd); //host to net long 
		iLen+=sizeof(int);	
		memcpy(pBuff+iLen,pack.filename,NAME_SIZE);
		iLen+=sizeof(pack.filename);
		memcpy(pBuff+iLen,pack.buf,BUF_SIZE);
		iLen+=sizeof(pack.buf);
		
		memcpy(&n.buf,pBuff,RCV_SIZE);
		pthread_mutex_lock(&t_mutex);			//（多线程）对任务列表packtask进行操作要加互斥锁 
		packtask.push_back(n);
		pthread_mutex_unlock(&t_mutex);
//		usleep(100000);			//单位是微妙，这里休眠0.1秒用于验证多线程
	}
	delete[] pBuff;
	fclose(fp);
	threadpool_add_task(&handler_pool, handle, arg);		//上述的文件读完后，调用handle来处理任务列表 
}

void recvfile(void *arg){					//将客户端上传的数据包加入任务列表 
//	printf("excuting recvfile\n");
	char *recvMsg=new char[RCV_SIZE];
	struct node n;
	int readlen;
	struct my_event *ev=(struct my_event*)arg;
	while((readlen=MyRecv(ev->fd,recvMsg,sizeof(package)))>0){
		memcpy(&n.buf,recvMsg,RCV_SIZE);
		pthread_mutex_lock(&t_mutex);
		packtask.push_back(n); 
		pthread_mutex_unlock(&t_mutex);
//		sleep(1);  这个休眠用户验证多线程 
	} 
	threadpool_add_task(&handler_pool, handle, arg);
}

void handle(void *arg){			//对任务列表进行操作 
	int count=0;
	struct my_event *ev=(struct my_event*)arg;
	while(!packtask.empty()){
	//	printf("excuting handle!\n");
		pthread_mutex_lock(&t_mutex);
		int type;
		struct node n;
		n=packtask.front();
		packtask.pop_front();
		pthread_mutex_unlock(&t_mutex);
		memcpy(&type,n.buf,sizeof(int));
		type=(int)ntohl(type);
	
  	//	printf("type:%d\n",type);
		if(type==0){			//这个包要写到服务器本地文件里 （客户端上传文件） 
			struct package pack;
			memset(pack.filename,0,sizeof(pack.filename));
			memset(pack.buf,0,sizeof(pack.buf));
			 int iLen=0;
			memcpy(&pack.type,n.buf+iLen,sizeof(int));
			pack.type=ntohl(pack.type);
			iLen+=sizeof(int);
			
			memcpy(&pack.size,n.buf+iLen,sizeof(int));
			pack.size=ntohl(pack.size);
			iLen+=sizeof(int);
	//		printf("pack.size:%d    ",pack.size);		
	
			memcpy(&pack.fd,n.buf+iLen,sizeof(int));
			pack.fd=ntohl(pack.fd);
			iLen+=sizeof(int);
	
			memcpy(&pack.filename,n.buf+iLen,sizeof(pack.filename));
			iLen+=sizeof(pack.filename);
	//		printf("pack.filename:%s\n",pack.filename);
			
			memcpy(&pack.buf,n.buf+iLen,sizeof(pack.buf));
			iLen+=sizeof(pack.buf);
			
			char part_file[NAME_SIZE];
			memset(part_file,0,sizeof(part_file));
			getOper(pack.filename,part_file);  //分割出文件名，不如e:\\aaa.txt，part_file=aaa.txt 
			string path=PATH;
			path+=part_file;
	//		printf("path: %s\n",path.c_str());
			const char *p=path.c_str();
			FILE* fp;
			if((fp=fopen(p,"a"))==NULL){
				printf("cannot open file!\n");
				return;
			}
				if(pack.size>0)
			{
//				printf("writing!\n");
				fwrite(pack.buf,sizeof(char),pack.size,fp);
			}
			fflush(fp);
			
		}
		else if(type==1){		//（客户端下载文件） 
//		printf("type=1\n");
			++count;
			int fd;
			int iLen=2*sizeof(int);
			memcpy(&fd,n.buf+iLen,sizeof(int));
  
			if(MySend(fd,n.buf,RCV_SIZE)>0){
	//			usleep(100000);
	//			printf("fd:%d     %d send successfully!\n",fd,count);
			}
		}
	}
	
}

void eventset(struct my_event* ev,int fd,void (*call_back)(int,int,void *),void *arg){
	ev->fd=fd;
	ev->events=0;
	ev->call_back=call_back;
	ev->arg=arg;
	ev->status=0;
	ev->last_active=time(NULL); 
	return;
}

void eventadd(int efd,int events,struct my_event* ev){
	struct epoll_event epv={0,{0}};
	int op;
	epv.data.ptr=ev;
	epv.events=ev->events=events;
	
	if(ev->status==1){
		op=EPOLL_CTL_MOD;
		
	}else{
		op=EPOLL_CTL_ADD;
		ev->status=1;
	}
	
	if(epoll_ctl(g_efd,op,ev->fd,&epv)<0){
		printf("event add failed! fd:%d  events:%d",ev->fd,events);
	}else{
		printf("event add successfully!efd:%d  fd:%d  events:%d\n",efd,ev->fd,events);
	}
	
	return;
	
}

void eventdel(int efd,struct my_event *ev){
	struct epoll_event epv={0,{0}};
	if(ev->status!=1)
		return;
	epv.data.ptr=ev;
	ev->status=0;
	epoll_ctl(efd,EPOLL_CTL_DEL,ev->fd,&epv);
	
	return; 
}

void acceptconn(int lfd,int events,void *arg){
	struct sockaddr_in cliaddr;
	socklen_t len=sizeof(cliaddr);
	int acceptfd,i;
	
	if((acceptfd=accept(lfd,(struct sockaddr*)&cliaddr,&len))==-1){
		perror("accept error:");
		return;
	}else{
		printf("accept a new client:%s,%d\n",inet_ntoa(cliaddr.sin_addr),cliaddr.sin_port);
	}
	
	do{
		for(i=0;i<MAX_EVENTS;i++){		//找到某个不在树上的g_events[i]
			if(g_events[i].status==0)
				break;
			}
			if(i==MAX_EVENTS){
				printf("cannot accept more client!\n");
				break;
			} 
			
			int flag=0;
			if((flag=fcntl(acceptfd,F_SETFL,O_NONBLOCK))<0){
				printf("fcntl nonblock failed:%s\n ",strerror(errno));
				break;
			}
	//	void eventset(struct my_event* ev,int fd,void (*call_back)(int ,int ,void *),void *arg)		
			eventset(&g_events[i],acceptfd,recvdata,&g_events[i]);   //将事件注册到这个新连接的cfd上的
	//  void eventadd(int efd,int events,struct my_event* ev)
			eventadd(g_efd,EPOLLIN,&g_events[i]);				//将g_events[i].fd（也就是cfd）添加到g_efd中,让g_efd来管理 g_events[i].fd
			
	}while(0);
	
}

void initlistensocket(int efd,short port){
	int lfd=socket(AF_INET,SOCK_STREAM,0);
	printf("lfd:%d\n",lfd);
	fcntl(lfd,F_SETFL,O_NONBLOCK);		//将socket设置为非阻塞
	
	int reuse=1;
	setsockopt(lfd,SOL_SOCKET,SO_REUSEADDR,(const void*)&reuse,sizeof(reuse));		//设置端口在TIME_WAIT状态下可以重用
	

	
	struct sockaddr_in seraddr;
	memset(&seraddr,0,sizeof(seraddr));
	
	seraddr.sin_family=AF_INET;
	seraddr.sin_port=htons(port);
	seraddr.sin_addr.s_addr=htonl(INADDR_ANY);
	
	if(	bind(lfd,(struct sockaddr*)&seraddr,sizeof(seraddr))<0)
		perror("bind error:");
	else
		printf("bind ok!\n");
	if(listen(lfd,LISNUM)<0)
		perror("listen error:");
	else
		printf("listen ok!\n");

 
	//	void eventset(struct my_event* ev,int fd,void (*call_back)(int ,int ,void *),void *arg)
			eventset(&g_events[MAX_EVENTS],lfd,acceptconn,&g_events[MAX_EVENTS]);		//把监听的lfd（只有一个）设置在g_events的最后一个元素中 g_events[MAX_EVENTS]
	//		//  void eventadd(int efd,int events,struct my_event* ev)
				eventadd(efd,EPOLLIN,&g_events[MAX_EVENTS]);			//将 g_events[MAX_EVENTS] 加入到efd，事件为EPOLLIN 
	
	return;
	
}



int main(int argc,char *argv[]){
	
	unsigned short port=SERV_PORT;
	
	if(argc == 2)
		port=atoi(argv[1]);  //如果命令行输入了端口，就按输入的端口来
	
	g_efd=epoll_create(MAX_EVENTS+1);
	if(g_efd<0)
		printf("create efd error:%s",strerror(errno));
	else{
		printf("g_efd:%d",g_efd);
	}
	
	initlistensocket(g_efd,port);		//初始化监听socket
	
	
	threadpool_init(&pool, POOL_NUM);		//初始化线程池 
	threadpool_init(&handler_pool, POOL_NUM-1);
	struct epoll_event events[MAX_EVENTS+1];
	
	int checkpos=0,i;
	while(1){		//心跳模块 
		
		long now=time(NULL);
	/*	for(i=0;i<100;i++,checkpos++){
			if(checkpos==MAX_EVENTS)
				checkpos=0;
			if(g_events[checkpos].status!=1)
				continue;
			
			long duration=now-g_events[checkpos].last_active;		//客户端不活跃的时间
			
			if(duration>=60){						//如果超过一分钟就关闭客户端连接，别让其占资源 
				close(g_events[checkpos].fd);
				printf("fd=%d timeout",g_events[checkpos].fd);
				eventdel(g_efd,&g_events[checkpos]);
			} 
		}
*/
		
		int nfd=epoll_wait(g_efd,events,MAX_EVENTS+1,-1);		//nfd表示有事件的fd的数目，events用来从内核中得到有事件的epoll_event的集合 
		if(nfd<0){
			printf("epoll_wait error!\n");
			break;
		}
		
		for(i=0;i<nfd;i++){
			struct my_event* ev=(struct my_event *)events[i].data.ptr;
			
			if((events[i].events & EPOLLIN) && (ev->events & EPOLLIN))
				ev->call_back(ev->fd,events[i].events,ev->arg);
		//	if((events[i].events & EPOLLOUT) && (ev->events & EPOLLOUT))
		//		ev->call_back(ev->fd,events[i].events,ev->arg);	
			
		}
		
		
	} 
	threadpool_destroy(&pool);
	threadpool_destroy(&handler_pool);
	return 0;
}


