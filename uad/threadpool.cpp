#include"threadpool.h"
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<errno.h>
#include<time.h>

//#include "threadpool.h"
#include <unistd.h>
//#include <stdlib.h>
//#include <stdio.h>

//https://www.cnblogs.com/yangang92/p/5485868.html
//https://bbs.csdn.net/topics/390642622

void *thread_routine(void *arg){
	struct timespec abstime;
	int timeout;
	threadpool_t *pool=(threadpool_t *)arg;
	printf("thread %d is starting          poolsize:%d\n",(int)pthread_self(),pool->max_threads);
	
	while(1){
		timeout=0;
		condition_lock(&pool->ready);
		pool->idle++;
		while(pool->first==NULL&&!pool->quit){	//如果没有任务要执行并且线程池没有被销毁则等待 
			printf("thread %d is waiting      poolsize:%d\n",(int)pthread_self(),pool->max_threads);
			clock_gettime(CLOCK_REALTIME,&abstime);
			abstime.tv_sec+=10;
			int status;
			status=condition_timewait(&pool->ready,&abstime);
			if(status == ETIMEDOUT){
				printf("thread %d wait time out\n",(int)pthread_self());
				timeout=1;
				break;
			}
		}
		
		pool->idle--;
		
		if(pool->first!=NULL){	//如果有任务要执行的话 
			task_t *t=pool->first;		//将队头任务取出来 
			pool->first=t->next;		//设置队头指针 
			condition_unlock(&pool->ready);	//这时候这个任务就被唯一一个线程占有了，可以释放锁了 
			t->run(t->arg);		//执行任务 
			free(t);			//记得释放指针，不然会内存泄漏 
			condition_lock(&pool->ready);	
		}
		
		if(pool->quit && pool->first==NULL){ //退出线程池，并且没任务要执行了 
			pool->counter--;		//当前工作的线程数-1 
			if(pool->counter==0){
				condition_signal(&pool->ready);
			}
			condition_unlock(&pool->ready);
			break;
		}
		if(timeout==1){  //超时了都还没有任务要执行，那么就跳出循环， 这个线程也因为这个函数结束而结束了 
			pool->counter--;
			condition_unlock(&pool->ready);
			break;
		}
		condition_unlock(&pool->ready);
			
	}
	printf("thread %d is exiting\n",(int)pthread_self());
	return NULL;
} 

void threadpool_init(threadpool_t *pool,int threads){
	condition_init(&pool->ready);
	pool->first=NULL;
	pool->last=NULL;
	pool->counter=0;
	pool->idle=0;
	pool->max_threads=threads;
	pool->quit=0;   
}

void threadpool_add_task(threadpool_t *pool,void (*run)(void *arg),void *arg){
	task_t *newtask=(task_t *)malloc(sizeof(task_t));
	newtask->run=run;
	newtask->arg=arg; 
	newtask->next=NULL;	//新加的任务放在队尾 
	
	condition_lock(&pool->ready);
	
	if(pool->first==NULL){
		pool->first=newtask;
	}else{
		pool->last->next=newtask;
	}
	pool->last=newtask;
	
	if(pool->idle>0){
		condition_signal(&pool->ready);
	}else if(pool->counter<pool->max_threads){
		pthread_t tid;
		pthread_create(&tid,NULL,thread_routine,pool);
		pool->counter++;
	}
	
	condition_unlock(&pool->ready);
	
} 

void threadpool_destroy(threadpool_t *pool){
	if(pool->quit)
		return;
	
	condition_lock(&pool->ready);
	pool->quit=1;
	if(pool->counter>0){
		if(pool->idle>0)
			condition_broadcast(&pool->ready);
			
		while(pool->counter)
			condition_wait(&pool->ready);
	}
	condition_unlock(&pool->ready);
	condition_destroy(&pool->ready);
}




/*void* mytask(void *arg)
{
	int i=2;
	while(i--){
		printf("thread %d is working on task %d\n", (int)pthread_self(), *(int*)arg);
    	sleep(1);
    	
	} 
    free(arg);
    return NULL;
}

//测试代码
int main(void)
{
    threadpool_t pool;
    //初始化线程池，最多三个线程
    threadpool_init(&pool, 3);
    int i;
    //创建十个任务
    for(i=0; i < 10; i++)
    {
        int *arg = malloc(sizeof(int));
        *arg = i;
        threadpool_add_task(&pool, mytask, arg);
        
    }
    threadpool_destroy(&pool);
    return 0;
}
*/





